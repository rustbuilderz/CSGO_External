#pragma once
#include "imgui.h"
#include "memory.h"
bool WorldToScreen(const Vector3& pos, ImVec2& out);

void DrawSkeleton(
    ImDrawList* dl,
    int entIdx,
    ImU32 color = IM_COL32(255, 255, 255, 255),
    float thickness = 1.0f);

void DrawName(
    ImDrawList* dl,
    int entIdx,
    ImU32 color = IM_COL32(255, 255, 255, 255));

void RenderESPBox(ImDrawList* dl, int entIdx);
void RenderHealthBar(ImDrawList* dl, int entIdx);
void RenderTeamText(ImDrawList* dl, int entIdx);
