#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <string>

// maximum entities
static constexpr int MAX_ENTITY_COUNT = 1000;

// simple 3D point
struct Vector3 { float x, y, z; };

// on‐screen coordinate
struct ScreenPos { float x, y; };

// world‐space data (filled by UpdateEntityData)
extern Vector3   entityPositions[MAX_ENTITY_COUNT];
extern int       entityLifeState[MAX_ENTITY_COUNT];
extern int       entityHealth[MAX_ENTITY_COUNT];
extern int       entityTeam[MAX_ENTITY_COUNT];
// screen‐space buffers (filled by UpdateEntityData)
extern ScreenPos entityFootPos[MAX_ENTITY_COUNT];
extern ScreenPos entityHeadPos[MAX_ENTITY_COUNT];

// how many valid entries above
extern int       entityCount;

// view matrix (4×4 float)
extern float     viewMatrix[16];

// these two hold the raw pointers for each entity
extern uintptr_t entityPawnPtr[MAX_ENTITY_COUNT];
extern uintptr_t entityBoneArray[MAX_ENTITY_COUNT];

// process handle for ReadProcessMemory
extern HANDLE    g_hProcess;

// Launches a background thread to keep these updated at ~60Hz
void StartMemoryThread();

// Performs one immediate batch update on this thread
void UpdateEntityData();

// Get a single bone world‐position into `out`
bool GetBonePosition(int entIdx, int boneIndex, Vector3& out);

// Get the name string of an entity
bool GetPlayerName(int entIdx, std::string& out);

bool ReadMem(uintptr_t src, void* dst, size_t sz);
bool GetEntityHeldWeapon(int entIdx, std::string& out);