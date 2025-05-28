#pragma once

#include <Windows.h>
#include "memory.h"    // for GetBonePosition, entityTeam[], entityCount, viewMatrix[], etc.
#include "imgui/imgui.h"

// user-configurable
extern bool  g_enableAimbot;
extern bool  g_aimbotHoldOnly;
extern int   g_aimbotKey;      // VK_*
extern float g_aimbotFOV;      // degrees
extern float g_aimbotSmooth;   // >1.0
extern bool g_drawFovCircle;

// must be called once after your device/ImGui init
void Aimbot_Init();

// call this every frame (before Render)
void Aimbot_Update();
void RenderAimbotSettings();
void DrawAimbotTargetMarker();
