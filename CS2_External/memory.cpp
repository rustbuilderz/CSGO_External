#include "memory.h"
#include "offsets.hpp"
#include <Windows.h>
#include <TlHelp32.h>
#include <thread>
#include <chrono>
#include <string>


// file-scope handle & base
static HANDLE    hProc = nullptr;
static uintptr_t clientBase = 0;

// offsets
using namespace cs2_dumper::offsets::client_dll;
constexpr auto OFF_ENTITY_LIST = dwEntityList;
constexpr auto OFF_VIEW_MATRIX = dwViewMatrix;
constexpr auto OFF_ORIGIN = m_vOldOrigin;
constexpr auto OFF_LIFESTATE = m_lifeState;
constexpr auto OFF_PLAYERPAWN = m_hPlayerPawn;
constexpr auto OFF_GAME_SCENE_NODE = m_pGameSceneNode;  // == 808
constexpr size_t OFF_HEALTH = 836;
constexpr size_t OFF_TEAM = 995;
constexpr auto OFF_DW_PLANTED_C4 = dwPlantedC4;
constexpr auto OFF_VEC_ABSORIGIN = m_vecAbsOrigin;      // ≈ 208
constexpr auto OFF_PLANTED_C4_HANDLE = m_bBombPlanted;
constexpr auto OFF_ACTIVE_WEAPON_HANDLE = m_hActiveWeapon;
constexpr auto OFF_PAWN_WEAPON_SERVICES = m_pWeaponServices;

// this offset was observed in other code: sceneNode + 0x1F0 → boneArray
static constexpr size_t OFF_BONE_ARRAY = 0x1F0;

// globals
Vector3   entityPositions[MAX_ENTITY_COUNT];
int       entityLifeState[MAX_ENTITY_COUNT];
int       entityHealth[MAX_ENTITY_COUNT];
int       entityTeam[MAX_ENTITY_COUNT];
ScreenPos entityFootPos[MAX_ENTITY_COUNT];
ScreenPos entityHeadPos[MAX_ENTITY_COUNT];
int       entityCount = 0;
float     viewMatrix[16];

// ← new storage for pointers
uintptr_t entityPawnPtr[MAX_ENTITY_COUNT] = {};
uintptr_t entityBoneArray[MAX_ENTITY_COUNT] = {};

uintptr_t entityEntPtr[MAX_ENTITY_COUNT] = {};

static constexpr size_t OFF_ENTITY_NAME = m_sSanitizedPlayerName;

// non‐inline ReadMem that uses our file‐scope hProc
bool ReadMem(uintptr_t src, void* dst, size_t sz) {
    SIZE_T rd = 0;
    return ReadProcessMemory(hProc, (LPCVOID)src, dst, sz, &rd) && rd == sz;
}


// simple ReadMem wrapper
inline bool ReadMem(HANDLE proc, uintptr_t src, void* dst, SIZE_T sz) {
    SIZE_T rd = 0;
    return ReadProcessMemory(proc, (LPCVOID)src, dst, sz, &rd) && rd == sz;
}

static DWORD GetProcId(const wchar_t* name) {
    PROCESSENTRY32W pe{ sizeof(pe) };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (!_wcsicmp(pe.szExeFile, name)) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

static uintptr_t GetModuleBaseAddress(DWORD pid, const wchar_t* mod) {
    MODULEENTRY32W me{ sizeof(me) };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (Module32FirstW(snap, &me)) {
        do {
            if (!_wcsicmp(me.szModule, mod)) {
                CloseHandle(snap);
                return reinterpret_cast<uintptr_t>(me.modBaseAddr);
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return 0;
}

void UpdateEntityData() {
    if (!hProc) {
        DWORD pid = GetProcId(L"cs2.exe");
        if (!pid) return;
        clientBase = GetModuleBaseAddress(pid, L"client.dll");
        hProc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!hProc) return;
    }

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // read view matrix
    ReadMem(hProc, clientBase + OFF_VIEW_MATRIX, viewMatrix, sizeof(viewMatrix));

    // read entity list pointer
    uintptr_t listPtr = 0;
    ReadMem(hProc, clientBase + OFF_ENTITY_LIST, &listPtr, sizeof(listPtr));
    if (!listPtr) return;

    int count = 0;
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i) {
        // 1) page resolution
        int pageIdx = (i & 0x7FFF) >> 9;
        int inPageIdx = i & 0x1FF;
        uintptr_t pagePtr = 0;
        ReadMem(hProc, listPtr + 0x10 + pageIdx * 8, &pagePtr, sizeof(pagePtr));
        if (!pagePtr) continue;

        // 2) entry → entPtr
        uintptr_t entPtr = 0;
        ReadMem(hProc, pagePtr + inPageIdx * 0x78, &entPtr, sizeof(entPtr));
        if (!entPtr) continue;

        // 3) store valid entPtr
        entityEntPtr[count] = entPtr;

        // 4) follow pawn handle → pawnPtr
        uintptr_t handle = 0;
        ReadMem(hProc, entPtr + OFF_PLAYERPAWN, &handle, sizeof(handle));
        if (!handle) continue;
        int pPageIdx = (handle & 0x7FFF) >> 9;
        int pInPageIdx = handle & 0x1FF;
        uintptr_t pawnPage = 0;
        ReadMem(hProc, listPtr + 0x10 + pPageIdx * 8, &pawnPage, sizeof(pawnPage));
        if (!pawnPage) continue;
        uintptr_t pawnPtr = 0;
        ReadMem(hProc, pawnPage + pInPageIdx * 0x78, &pawnPtr, sizeof(pawnPtr));
        if (!pawnPtr) continue;

        // 5) read origin/state …
        Vector3 pos{}; ReadMem(hProc, pawnPtr + OFF_ORIGIN, &pos, sizeof(pos));
        if (pos.x == 0 && pos.y == 0 && pos.z == 0) continue;
        uint8_t ls;    ReadMem(hProc, pawnPtr + OFF_LIFESTATE, &ls, sizeof(ls));
        int     hl;    ReadMem(hProc, pawnPtr + OFF_HEALTH, &hl, sizeof(hl));
        int     tm;    ReadMem(hProc, pawnPtr + OFF_TEAM, &tm, sizeof(tm));

        entityPositions[count] = pos;
        entityLifeState[count] = ls;
        entityHealth[count] = hl;
        entityTeam[count] = tm;
        entityPawnPtr[count] = pawnPtr;

        // 6) scene-node → boneArray …
        uintptr_t sceneNode = 0;
        ReadMem(hProc, pawnPtr + OFF_GAME_SCENE_NODE, &sceneNode, sizeof(sceneNode));
        uintptr_t boneArr = 0;
        if (sceneNode)
            ReadMem(hProc, sceneNode + OFF_BONE_ARRAY, &boneArr, sizeof(boneArr));
        entityBoneArray[count] = boneArr;

        // 7) project foot & head …
        Vector3 headW = { pos.x, pos.y, pos.z + 75.f };
        auto proj = [&](const Vector3& w, ScreenPos& o) {
            float cx = w.x * viewMatrix[0] + w.y * viewMatrix[1] + w.z * viewMatrix[2] + viewMatrix[3];
            float cy = w.x * viewMatrix[4] + w.y * viewMatrix[5] + w.z * viewMatrix[6] + viewMatrix[7];
            float w4 = w.x * viewMatrix[12] + w.y * viewMatrix[13] + w.z * viewMatrix[14] + viewMatrix[15];
            if (w4 < 0.01f) { o.x = o.y = -1; return; }
            float inv = 1.f / w4;
            o.x = (cx * inv * 0.5f + 0.5f) * screenW;
            o.y = (1.f - (cy * inv * 0.5f + 0.5f)) * screenH;
            };
        proj(pos, entityFootPos[count]);
        proj(headW, entityHeadPos[count]);

        ++count;
    }

    entityCount = count;
}

bool GetPlayerName(int entIdx, std::string& out) {
    if (entIdx < 0 || entIdx >= entityCount) return false;
    uintptr_t entPtr = entityEntPtr[entIdx];
    if (!entPtr || !hProc) return false;

    // read the pointer to the unicode/UTF-8 string
    uintptr_t namePtr = 0;
    if (!ReadMem(hProc, entPtr + OFF_ENTITY_NAME, &namePtr, sizeof(namePtr)))
        return false;

    // read up to 64 or 128 bytes from that pointer
    char buffer[128] = {};
    if (!ReadMem(hProc, namePtr, buffer, sizeof(buffer)))
        return false;

    // trim at first null
    out.assign(buffer, strnlen_s(buffer, sizeof(buffer)));
    return !out.empty();
}
bool GetBonePosition(int entIdx, int boneIndex, Vector3& out) {
    if (entIdx < 0 || entIdx >= entityCount) return false;
    uintptr_t ba = entityBoneArray[entIdx];
    if (!ba || !hProc) return false;
    // each entry is 32 bytes, first 12 bytes = Vector3
    return ReadMem(hProc, ba + boneIndex * 32, &out, sizeof(out));
}

bool GetEntityHeldWeapon(int entIdx, std::string& out) {
    if (entIdx < 0 || entIdx >= entityCount)
        return false;

    // 1) grab the pawn pointer you stored in UpdateEntityData
    uintptr_t pawn = entityPawnPtr[entIdx];
    if (!pawn) return false;

    // 2) read the active-weapon handle (high bits = unique object id, low 12 bits = index)
    uint32_t handle = 0;
    if (!ReadMem(pawn + OFF_ACTIVE_WEAPON_HANDLE, &handle, sizeof(handle)))
        return false;

    int weaponIdx = handle & 0x0FFF; // low 12 bits
    if (weaponIdx < 0 || weaponIdx >= entityCount)
        return false;

    // 3) now we already have the entity pointer for every slot, so:
    uintptr_t weaponEnt = entityEntPtr[weaponIdx];
    if (!weaponEnt) return false;

    // 4) read its sanitized-name pointer
    uintptr_t namePtr = 0;
    if (!ReadMem(weaponEnt + OFF_ENTITY_NAME, &namePtr, sizeof(namePtr)) || !namePtr)
        return false;

    // 5) read up to 64 chars
    char buf[64] = {};
    if (!ReadMem(namePtr, buf, sizeof(buf)))
        return false;

    out = buf;
    return !out.empty();
}

void StartMemoryThread() {
    std::thread([]() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        while (true) {
            UpdateEntityData();
            std::this_thread::sleep_for(std::chrono::milliseconds(7));
        }
        }).detach();
}
