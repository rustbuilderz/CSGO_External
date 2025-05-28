#include "render.h"
#include <map>
#include <string>
#include <vector>
#include "menu.h"
#include <iostream>    // at top of your .cpp
#include <windows.h>
#include "aimbot.h"

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

static void PrintToConsole(const std::string& utf8)
{
    // figure out how many wide chars we need
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return;

    // do the conversion
    std::wstring wbuf(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wbuf[0], wlen);

    // write it (omit the terminating null)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteConsoleW(hOut, wbuf.c_str(), wlen - 1, &written, nullptr);
    WriteConsoleW(hOut, L"\r\n", 2, &written, nullptr);
}


static inline ImU32 RainbowColor(float speed = 0.2f, unsigned char alpha = 255)
{
    float t = ImGui::GetTime() * speed;
    float hue = fmodf(t, 1.0f);                  // wrap [0…1)
    ImVec4 c = ImColor::HSV(hue, 1.0f, 1.0f);   // full saturation & value
    return ImGui::GetColorU32(ImVec4{ c.x, c.y, c.z, alpha / 255.0f });
}

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


void DrawSkeleton(ImDrawList* dl, int entIdx, ImU32 baseColor, float thickness) {
    if (g_teamCheck && entityTeam[entIdx] == entityTeam[0])
        return;
    if (entityLifeState[entIdx] == 2 || entityHealth[entIdx] <= 0)
        return;

    // pick white or rainbow
    ImU32 drawColor = g_rainbowMode
        ? RainbowColor(0.5f, 255)
        : baseColor;

    for (auto& chain : skeletonChains) {
        ImVec2 prev;
        bool havePrev = false;

        for (int boneIndex : chain) {
            Vector3 wp;
            if (!GetBonePosition(entIdx, boneIndex, wp)) {
                havePrev = false;
                break;
            }
            ImVec2 sp;
            if (!WorldToScreen(wp, sp)) {
                havePrev = false;
                break;
            }
            if (havePrev)
                dl->AddLine(prev, sp, drawColor, thickness);
            prev = sp;
            havePrev = true;
        }
    }
}


void DrawName(ImDrawList* dl, int entIdx, ImU32 baseColor) {
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

    // pick white or rainbow
    ImU32 drawColor = g_rainbowMode
        ? RainbowColor(0.5f, 255)   // half‐speed rainbow, fully opaque
        : baseColor;

    // 4) Draw the text
    dl->AddText(textPos, drawColor, name.c_str());
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
    ImVec2 p0(head.x - w * 0.5f, head.y), p1(p0.x + w, p0.y + h);

    // pick white or rainbow
    ImU32 drawColor = g_rainbowMode
        ? RainbowColor(0.5f, 255)
        : IM_COL32(255, 255, 255, 255);

    dl->AddRect(p0, p1, drawColor, /*rounding=*/0, /*flags=*/0, /*thickness=*/1.5f);
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
    // early out if same team or dead
    if (g_teamCheck && entityTeam[entIdx] == entityTeam[0]) return;
    if (entityLifeState[entIdx] == 2 || entityHealth[entIdx] <= 0) return;

    // project foot & head
    ImVec2 foot, head;
    if (!WorldToScreen(entityPositions[entIdx], foot)) return;
    Vector3 hpos = entityPositions[entIdx];
    hpos.z += 75.0f;
    if (!WorldToScreen(hpos, head)) return;

    // compute box size
    float height = foot.y - head.y;
    if (height <= 0) return;
    float width = height * 0.5f;

    // build the text
    char buf[16];
    sprintf_s(buf, "T:%d", entityTeam[entIdx]);

    // pick white or rainbow
    ImU32 drawColor = g_rainbowMode
        ? RainbowColor(0.5f, 255)
        : IM_COL32(255, 255, 255, 255);

    // position just to the right of the box
    ImVec2 textPos{ head.x + width * 0.5f + 2, head.y };

    // draw
    dl->AddText(textPos, drawColor, buf);
}


void RenderWeaponName(ImDrawList* dl, int entIdx)
{
    // early out if dead or same team
    if (entityLifeState[entIdx] == 2 || entityHealth[entIdx] <= 0)
        return;
    if (g_teamCheck && entityTeam[entIdx] == entityTeam[0])
        return;

    // project foot & head so we can position the text
    ImVec2 foot, head;
    if (!WorldToScreen(entityPositions[entIdx], foot)) return;
    Vector3 hpos = entityPositions[entIdx];
    hpos.z += 75.0f;
    if (!WorldToScreen(hpos, head)) return;

    // compute box corners
    float h = foot.y - head.y;
    if (h <= 0) return;
    float w = h * 0.5f;
    ImVec2 p0{ head.x - w * 0.5f, head.y };
    ImVec2 p1{ p0.x + w,          p0.y + h };

    // read the weapon name from memory
    std::string weapon;
    if (!GetEntityHeldWeapon(entIdx, weapon))
        return;

    // print to your console (Unicode-safe)
    PrintToConsole(weapon);

    // draw it under the box in‐game
    dl->AddText(
        ImVec2(p0.x, p1.y + 2),
        IM_COL32(255, 255, 255, 255),
        weapon.c_str()
    );
}

// helper to convert FOV degrees → pixel radius (screen diagonal)
static float ComputeFovRadius(float fovDeg) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    float diag = sqrtf((float)sw * sw + (float)sh * sh);
    return (fovDeg / 180.0f) * (diag * 0.5f);
}

void DrawFOVCircle()
{
    if (!g_drawFovCircle || !g_enableAimbot)
        return;

    // get center of the screen
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    ImVec2 center{ sw * 0.5f, sh * 0.5f };

    // convert your FOV degrees to screen radius
    float radius = ComputeFovRadius(g_aimbotFOV);

    // grab a draw list (foreground so it's on top of everything)
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 color = g_rainbowMode
        ? RainbowColor(0.5f, 255)  // half‐speed rainbow, fully opaque
        : IM_COL32(255, 255, 255, 255);
    // draw a smooth circle with 64 segments
    dl->AddCircle(center, radius,
        color, 64, 1.0f);
}

void DrawCrosshair()
{
    ImGuiIO& io = ImGui::GetIO();
    const float cx = io.DisplaySize.x * 0.5f;
    const float cy = io.DisplaySize.y * 0.5f;
    const float halfLen = 8.0f;   // each arm is 16px total
    const float thickness = 1.0f;

    // pick white or rainbow
    ImU32 color = g_rainbowMode
        ? RainbowColor(0.5f, 255)   // half-speed hue, full alpha
        : IM_COL32(255, 255, 255, 255);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    // horizontal line
    dl->AddLine(
        ImVec2(cx - halfLen, cy),
        ImVec2(cx + halfLen, cy),
        color,
        thickness
    );
    // vertical line
    dl->AddLine(
        ImVec2(cx, cy - halfLen),
        ImVec2(cx, cy + halfLen),
        color,
        thickness
    );
}