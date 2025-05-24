#include "memory.h"
#include "offsets.hpp"       // a2x-dumped offsets
#include <Windows.h>
#include <TlHelp32.h>
#include <thread>
#include <chrono>

// — Definitions of all globals —
Vector3   entityPositions[MAX_ENTITY_COUNT];
int       entityLifeState[MAX_ENTITY_COUNT];
int       entityHealth[MAX_ENTITY_COUNT];
int       entityTeam[MAX_ENTITY_COUNT];
ScreenPos entityFootPos[MAX_ENTITY_COUNT];
ScreenPos entityHeadPos[MAX_ENTITY_COUNT];
int       entityCount = 0;
float     viewMatrix[16]; // 4×4

// — Helpers —
static DWORD GetProcId(const wchar_t* procName) {
    PROCESSENTRY32W e{ sizeof(e) };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (Process32FirstW(snap, &e)) {
        do {
            if (!_wcsicmp(e.szExeFile, procName)) {
                CloseHandle(snap);
                return e.th32ProcessID;
            }
        } while (Process32NextW(snap, &e));
    }
    CloseHandle(snap);
    return 0;
}

static uintptr_t GetModuleBaseAddress(DWORD pid, const wchar_t* modName) {
    MODULEENTRY32W m{ sizeof(m) };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (Module32FirstW(snap, &m)) {
        do {
            if (!_wcsicmp(m.szModule, modName)) {
                CloseHandle(snap);
                return reinterpret_cast<uintptr_t>(m.modBaseAddr);
            }
        } while (Module32NextW(snap, &m));
    }
    CloseHandle(snap);
    return 0;
}

template<typename T>
static bool ReadMem(HANDLE hProc, uintptr_t addr, T& out) {
    return ReadProcessMemory(hProc, reinterpret_cast<LPCVOID>(addr), &out, sizeof(out), nullptr) != 0;
}

// — Core update loop —
void UpdateEntityData() {
    static HANDLE    hProc = nullptr;
    static uintptr_t clientBase = 0;
    if (!hProc) {
        DWORD pid = GetProcId(L"cs2.exe");
        clientBase = GetModuleBaseAddress(pid, L"client.dll");
        hProc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    }

    // Offsets (verify OFF_VEC_ORIGIN in your dump!)
    constexpr auto OFF_ENTITY_LIST = cs2_dumper::offsets::client_dll::dwEntityList;
    constexpr auto OFF_VIEW_MATRIX = cs2_dumper::offsets::client_dll::dwViewMatrix;
    constexpr auto OFF_PLAYER_PAWN = 0x824;   // m_hPlayerPawn
    constexpr auto OFF_VEC_ORIGIN = 4900;    // m_vOldOrigin
    constexpr auto OFF_LIFE_STATE = cs2_dumper::offsets::client_dll::m_lifeState;
    constexpr auto OFF_HEALTH = 836;     // m_iHealth
    constexpr auto OFF_TEAM = 995;     // m_iTeamNum

    // Read view matrix
    ReadMem(hProc, clientBase + OFF_VIEW_MATRIX, viewMatrix);

    // Read entity list pointer
    uintptr_t listPtr = 0;
    ReadMem(hProc, clientBase + OFF_ENTITY_LIST, listPtr);

    Vector3   tmpPos[MAX_ENTITY_COUNT];
    ScreenPos tmpFoot[MAX_ENTITY_COUNT];
    ScreenPos tmpHead[MAX_ENTITY_COUNT];
    int       tmpLife[MAX_ENTITY_COUNT];
    int       tmpHealth[MAX_ENTITY_COUNT];
    int       tmpTeam[MAX_ENTITY_COUNT];
    int       count = 0;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // Iterate all entity slots
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i) {
        uintptr_t le = 0, en = 0, ph = 0, pe = 0, pw = 0;
        ReadMem(hProc, listPtr + ((i & 0x7FFF) >> 9) * 8 + 0x10, le);
        if (!le) continue;

        ReadMem(hProc, le + (i & 0x1FF) * 0x78, en);
        if (!en) continue;

        ReadMem(hProc, en + OFF_PLAYER_PAWN, ph);
        if (!ph) continue;

        ReadMem(hProc, listPtr + ((ph & 0x7FFF) >> 9) * 8 + 0x10, pe);
        if (!pe) continue;

        ReadMem(hProc, pe + (ph & 0x1FF) * 0x78, pw);
        if (!pw) continue;

        // Read world origin (filter zeroed entries)
        Vector3 pos{};
        ReadMem(hProc, pw + OFF_VEC_ORIGIN, pos);
        if (pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f) continue;

        // Head world pos
        Vector3 headWorld{ pos.x, pos.y, pos.z + 75.0f };

        // Read states
        uint8_t life8 = 0; ReadMem(hProc, pw + OFF_LIFE_STATE, life8);
        int     life = static_cast<int>(life8);
        int     health = 0; ReadMem(hProc, pw + OFF_HEALTH, health);
        int     team = 0; ReadMem(hProc, pw + OFF_TEAM, team);

        // Foot → screen
        {
            float clipX = pos.x * viewMatrix[0] + pos.y * viewMatrix[1] + pos.z * viewMatrix[2] + viewMatrix[3];
            float clipY = pos.x * viewMatrix[4] + pos.y * viewMatrix[5] + pos.z * viewMatrix[6] + viewMatrix[7];
            float w = pos.x * viewMatrix[12] + pos.y * viewMatrix[13] + pos.z * viewMatrix[14] + viewMatrix[15];
            if (w > 0.01f) {
                float invW = 1.0f / w;
                tmpFoot[count].x = ((clipX * invW) * 0.5f + 0.5f) * screenW;
                tmpFoot[count].y = (1.0f - ((clipY * invW) * 0.5f + 0.5f)) * screenH;
            }
        }
        // Head → screen
        {
            float clipX = headWorld.x * viewMatrix[0] + headWorld.y * viewMatrix[1] + headWorld.z * viewMatrix[2] + viewMatrix[3];
            float clipY = headWorld.x * viewMatrix[4] + headWorld.y * viewMatrix[5] + headWorld.z * viewMatrix[6] + viewMatrix[7];
            float w = headWorld.x * viewMatrix[12] + headWorld.y * viewMatrix[13] + headWorld.z * viewMatrix[14] + viewMatrix[15];
            if (w > 0.01f) {
                float invW = 1.0f / w;
                tmpHead[count].x = ((clipX * invW) * 0.5f + 0.5f) * screenW;
                tmpHead[count].y = (1.0f - ((clipY * invW) * 0.5f + 0.5f)) * screenH;
            }
        }

        tmpPos[count] = pos;
        tmpLife[count] = life;
        tmpHealth[count] = health;
        tmpTeam[count] = team;
        ++count;
    }

    // Commit globals
    entityCount = count;
    for (int j = 0; j < count; ++j) {
        entityPositions[j] = tmpPos[j];
        entityLifeState[j] = tmpLife[j];
        entityHealth[j] = tmpHealth[j];
        entityTeam[j] = tmpTeam[j];
        entityFootPos[j] = tmpFoot[j];
        entityHeadPos[j] = tmpHead[j];
    }
}

void StartMemoryThread() {
    std::thread([] {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        while (true) {
            UpdateEntityData();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        }).detach();
}
