#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>

// — Basic math & screen types —
struct Vector3 { float x, y, z; };
struct ScreenPos { float x, y; };

static constexpr int MAX_ENTITY_COUNT = 1000;

// — Raw world data (populated by UpdateEntityData) —
extern Vector3    entityPositions[MAX_ENTITY_COUNT];
extern int        entityLifeState[MAX_ENTITY_COUNT];
extern int        entityHealth[MAX_ENTITY_COUNT];
extern int        entityTeam[MAX_ENTITY_COUNT];

// — Screen‐space positions (foot & head) —
extern ScreenPos  entityFootPos[MAX_ENTITY_COUNT];
extern ScreenPos  entityHeadPos[MAX_ENTITY_COUNT];

extern int        entityCount;      // number of valid entities read
extern float      viewMatrix[16];   // 4×4 view/projection matrix

// — Memory‐reading & process helpers —

// returns PID of the given exe name (e.g. L"cs2.exe") or 0 if not found
DWORD GetProcId(const wchar_t* procName);

// returns base address of module 'modName' in process 'procId', or 0 on failure
uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName);

// reads sizeof(T) bytes from 'addr' in process hProc into out.
// returns true on success.
template<typename T>
inline bool ReadMem(HANDLE hProc, uintptr_t addr, T& out) {
    return ReadProcessMemory(
        hProc,
        reinterpret_cast<LPCVOID>(addr),
        &out,
        sizeof(T),
        nullptr
    ) != 0;
}

// — Entry points —

// Launches a background thread that continuously updates entityPositions, etc.
void StartMemoryThread();

// Immediately performs one UpdateEntityData pass on the calling thread
void UpdateEntityData();

// Launches the ESP overlay in its own thread
void StartESPThread();
int  RunESP();

DWORD GetProcId(const wchar_t* procName);
uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName);

