#pragma once

struct Vector3 { float x, y, z; };
struct ScreenPos { float x, y; };

static constexpr int MAX_ENTITY_COUNT = 10;

extern Vector3    entityPositions[MAX_ENTITY_COUNT];
extern int        entityLifeState[MAX_ENTITY_COUNT];
extern int        entityHealth[MAX_ENTITY_COUNT];
extern int        entityTeam[MAX_ENTITY_COUNT];

// NEW: precomputed screen positions
extern ScreenPos  entityFootPos[MAX_ENTITY_COUNT];
extern ScreenPos  entityHeadPos[MAX_ENTITY_COUNT];

extern int        entityCount;

// view matrix (4×4)
extern float      viewMatrix[16];

void UpdateEntityData();    // made public
void StartMemoryThread();
