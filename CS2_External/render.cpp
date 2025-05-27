#include "render.h"
#include <map>
#include <string>
#include <vector>
#include "menu.h"

// explicit bone → index map (no structured bindings needed here)
static const std::vector<std::vector<int>> skeletonChains = {
    // torso: pelvis → spine_2 → spine_1 → neck_0 → head
    { 0, 2, 4, 5, 6 },
    // left arm:    spine_1 → arm_upper_L → arm_lower_L → hand_L
    { 4, 8, 9, 10 },
    // right arm:   spine_1 → arm_upper_R → arm_lower_R → hand_R
    { 4, 13, 14, 15 },
    // left leg:    pelvis → leg_upper_L → leg_lower_L → ankle_L
    { 0, 22, 23, 24 },
    // right leg:   pelvis → leg_upper_R → leg_lower_R → ankle_R
    { 0, 25, 26, 27 }
};
static const std::vector<std::pair<int, int>> g_skeletonBones = {
    { 7, 6 },   // head -> neck
    { 6, 5 },   // neck -> chest
    { 5, 4 },   // chest -> pelvis
    // left arm
    { 5, 30 },  // chest -> left clavicle
    { 30, 31 }, // left clavicle -> left upper arm
    { 31, 32 }, // left upper arm -> left forearm
    { 32, 33 }, // left forearm -> left hand
    // right arm
    { 5, 20 },  // chest -> right clavicle
    { 20, 21 }, // right clavicle -> right upper arm
    { 21, 22 }, // right upper arm -> right forearm
    { 22, 23 }, // right forearm -> right hand
    // left leg
    { 4, 40 },  // pelvis -> left thigh
    { 40, 41 }, // left thigh -> left calf
    { 41, 42 }, // left calf -> left foot
    // right leg
    { 4, 50 },  // pelvis -> right thigh
    { 50, 51 }, // right thigh -> right calf
    { 51, 52 }, // right calf -> right foot
};

bool WorldToScreen(const Vector3& pos, ImVec2& out) {
    float clipX = pos.x * viewMatrix[0] + pos.y * viewMatrix[1] + pos.z * viewMatrix[2] + viewMatrix[3];
    float clipY = pos.x * viewMatrix[4] + pos.y * viewMatrix[5] + pos.z * viewMatrix[6] + viewMatrix[7];
    float w = pos.x * viewMatrix[12] + pos.y * viewMatrix[13] + pos.z * viewMatrix[14] + viewMatrix[15];
    if (w < 0.01f) return false;
    float invW = 1.0f / w;
    ImGuiIO& io = ImGui::GetIO();
    out.x = (clipX * invW * 0.5f + 0.5f) * io.DisplaySize.x;
    out.y = (1.0f - (clipY * invW * 0.5f + 0.5f)) * io.DisplaySize.y;
    return true;
}


void DrawSkeleton(ImDrawList* dl, int entIdx, ImU32 color, float thickness) {
    if (g_teamCheck && entityTeam[entIdx] == entityTeam[0])
        return;
    // don’t draw skeleton for dead or zero‐health entities
    if (entityLifeState[entIdx] == 2 || entityHealth[entIdx] <= 0)
        return;

    for (auto& chain : skeletonChains)
    {
        ImVec2 prev;
        bool havePrev = false;

        for (int boneIndex : chain)
        {
            Vector3 wp;
            if (!GetBonePosition(entIdx, boneIndex, wp))
            {
                havePrev = false;
                break;
            }

            ImVec2 sp;
            if (!WorldToScreen(wp, sp))
            {
                havePrev = false;
                break;
            }

            if (havePrev)
                dl->AddLine(prev, sp, color, thickness);

            prev = sp;
            havePrev = true;
        }
    }
}


void DrawName(ImDrawList* dl, int entIdx, ImU32 color) {
    if (g_teamCheck && entityTeam[entIdx] == entityTeam[0])
        return;
    if (entityLifeState[entIdx] == 2 || entityHealth[entIdx] <= 0)
        return;
    // 1) Read the name from memory
    std::string name;
    if (!GetPlayerName(entIdx, name) || name.empty())
        return;

    // 2) Grab the already‐projected head position
    ScreenPos hp = entityHeadPos[entIdx];
    if (hp.x < 0 || hp.y < 0)  // off‐screen
        return;

    // 3) Offset it a bit above the box
    ImVec2 textPos(hp.x, hp.y - 12.0f);

    // 4) Draw the text
    dl->AddText(textPos, color, name.c_str());
}


// helper to project world→screen
extern bool WorldToScreen(const Vector3&, ImVec2&);

void RenderESPBox(ImDrawList* dl, int entIdx) {
    if (g_teamCheck && entityTeam[entIdx] == entityTeam[0])
        return;
    if (entityLifeState[entIdx] == 2 || entityHealth[entIdx] <= 0)
        return;
    ImVec2 foot, head;
    if (!WorldToScreen(entityPositions[entIdx], foot)) return;
    Vector3 hpos = entityPositions[entIdx]; hpos.z += 75.0f;
    if (!WorldToScreen(hpos, head)) return;

    float h = foot.y - head.y;
    if (h <= 0) return;
    float w = h * 0.5f;

    ImVec2 p0(head.x - w * 0.5f, head.y);
    ImVec2 p1(p0.x + w, p0.y + h);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 255), 0, 0, 1.5f);
}

void RenderHealthBar(ImDrawList* dl, int entIdx) {
    if (g_teamCheck && entityTeam[entIdx] == entityTeam[0])
        return;
    if (entityLifeState[entIdx] == 2 || entityHealth[entIdx] <= 0)
        return;
    // must be called *after* RenderESPBox so p0,p1 are still valid:
    ImVec2 foot, head;
    WorldToScreen(entityPositions[entIdx], foot);
    Vector3 hpos = entityPositions[entIdx]; hpos.z += 75.0f;
    WorldToScreen(hpos, head);

    float h = foot.y - head.y;
    if (h <= 0) return;
    float w = h * 0.5f;
    ImVec2 p0(head.x - w * 0.5f, head.y), p1(p0.x + w, p0.y + h);

    float bw = 4.0f;
    ImVec2 hb0(p0.x - bw - 1, p0.y), hb1(p0.x - 1, p1.y);
    // background
    dl->AddRectFilled(hb0, hb1, IM_COL32(0, 0, 0, 150));
    // green fill
    float frac = entityHealth[entIdx] / 100.0f;
    ImVec2 hf0(hb0.x, hb1.y - (hb1.y - hb0.y) * frac);
    dl->AddRectFilled(hf0, hb1, IM_COL32(0, 255, 0, 255));
}

void RenderTeamText(ImDrawList* dl, int entIdx) {
    if (g_teamCheck && entityTeam[entIdx] == entityTeam[0])
        return;
    if (entityLifeState[entIdx] == 2 || entityHealth[entIdx] <= 0)
        return;
    ImVec2 foot, head;
    WorldToScreen(entityPositions[entIdx], foot);
    Vector3 hpos = entityPositions[entIdx]; hpos.z += 75.0f;
    WorldToScreen(hpos, head);

    float h = foot.y - head.y;
    if (h <= 0) return;
    float w = h * 0.5f;
    ImVec2 p0(head.x - w * 0.5f, head.y);

    char buf[16];
    sprintf_s(buf, "T:%d", entityTeam[entIdx]);
    dl->AddText(ImVec2(p0.x + w + 2, p0.y), IM_COL32(255, 255, 255, 255), buf);
}
