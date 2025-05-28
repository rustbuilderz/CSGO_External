#include "aimbot.h"
#include "render.h"      // brings in bool WorldToScreen(const Vector3&, ImVec2&)
#include <Windows.h>
#include <cmath>
#include <winuser.h>
#include "imgui/imgui.h"
#include "memory.h"
#include "menu.h"
#include "offsets.hpp"
//––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
// User‐configurable globals (must match your aimbot.h externs)
bool  g_enableAimbot = false;
bool  g_aimbotHoldOnly = true;
int   g_aimbotKey = 'X';       // default = X
float g_aimbotFOV = 10.0f;     // degrees
float g_aimbotSmooth = 5.0f;      // larger = slower

// Internal mapping for ImGui combo
static const struct { const char* name; int vk; } s_keyList[] = {
    { "X"      , 'X'        },
    { "Ctrl"   , VK_CONTROL },
    { "Shift"  , VK_SHIFT   },
    { "Mouse4" , VK_XBUTTON1},
    { "Mouse5" , VK_XBUTTON2},
};
static int s_keyCount = IM_ARRAYSIZE(s_keyList);
static int s_currentKeyIdx = 0;      // will be synced in Aimbot_Init()

void Aimbot_Init() {

    // pick the right combo entry for our starting key
    for (int i = 0; i < s_keyCount; i++) {
        if (s_keyList[i].vk == g_aimbotKey) {
            s_currentKeyIdx = i;
            break;
        }
    }
}

// helper: convert FOV degrees → pixel radius (screen diagonal)
static float FovToRadius(float fovDeg) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    float diag = std::sqrt(float(sw * sw + sh * sh));
    return (fovDeg / 180.0f) * (diag * 0.5f);
}


void Aimbot_Update() {
    static float accX = 0.0f, accY = 0.0f;

    if (!g_enableAimbot)
        return;
    if (g_aimbotHoldOnly && !(GetAsyncKeyState(g_aimbotKey) & 0x8000))
        return;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    float cx = sw * 0.5f;
    float cy = sh * 0.5f;
    float best = FovToRadius(g_aimbotFOV);
    ImVec2 bestPos{ 0, 0 };
    bool found = false;

    constexpr int HEAD_BONE = 6;
    constexpr float PREDICTION_TIME = 0.04f; // seconds ahead

    for (int i = 1; i < entityCount; ++i) {
        // skip dead or zero‐health
        if (entityLifeState[i] == 2 || entityHealth[i] <= 0)
            continue;
        // optional team check
        if (g_teamCheck && entityTeam[i] == entityTeam[0])
            continue;

        // 1) Get current head position
        Vector3 head;
        if (!GetBonePosition(i, HEAD_BONE, head))
            continue;

        // 2) Predict movement
        Vector3 vel;
        ReadMem(entityPawnPtr[i] + cs2_dumper::offsets::client_dll::m_vecVelocity, &vel, sizeof(vel));
        head.x += vel.x * PREDICTION_TIME;
        head.y += vel.y * PREDICTION_TIME;
        head.z += vel.z * PREDICTION_TIME;

        // 3) Project to screen
        ImVec2 screen;
        if (!WorldToScreen(head, screen))
            continue;

        // 4) FOV check
        float dx = screen.x - cx;
        float dy = screen.y - cy;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < best) {
            best = dist;
            bestPos = screen;
            found = true;
        }
    }

    if (!found)
        return;

    // 5) Compute smooth delta
    float rawX = (bestPos.x - cx) / g_aimbotSmooth;
    float rawY = (bestPos.y - cy) / g_aimbotSmooth;

    // 6) Accumulate sub‐pixel remainders
    accX += rawX;
    accY += rawY;

    // 7) Extract integer movement
    int moveX = int(accX);
    int moveY = int(accY);

    // 8) Keep only the fractional remainder
    accX -= moveX;
    accY -= moveY;

    // 9) Send mouse move if non-zero
    if (moveX != 0 || moveY != 0) {
        mouse_event(MOUSEEVENTF_MOVE, SHORT(moveX), SHORT(moveY), 0, 0);
    }
}




// now define your settings renderer so menu.cpp can link to it
void RenderAimbotSettings() {
    if (!ImGui::CollapsingHeader("Aimbot")) return;
    ImGui::Checkbox("Enable Aimbot", &g_enableAimbot);
    ImGui::Checkbox("Hold To Aim Only", &g_aimbotHoldOnly);
    ImGui::Checkbox("Draw FOV Circle", &g_drawFovCircle);
    ImGui::Checkbox("Draw a '+' Crosshair", &g_drawCrosshair);

    // build a name array from s_keyList
    static const char* names[IM_ARRAYSIZE(s_keyList)];
    for (int i = 0; i < s_keyCount; ++i)
        names[i] = s_keyList[i].name;

    if (ImGui::Combo("Aim Key", &s_currentKeyIdx, names, s_keyCount))
        g_aimbotKey = s_keyList[s_currentKeyIdx].vk;

    ImGui::SliderFloat("FOV (deg)", &g_aimbotFOV, 1.0f, 30.0f, "%.1f");
    ImGui::SliderFloat("Smoothness", &g_aimbotSmooth, 1.0f, 20.0f, "%.1f");
}

void DrawAimbotTargetMarker()
{
    if (!g_enableAimbot)
        return;

    // screen center
    ImGuiIO& io = ImGui::GetIO();
    const float cx = io.DisplaySize.x * 0.5f;
    const float cy = io.DisplaySize.y * 0.5f;

    // same selection logic as in Aimbot_Update()
    float bestRadius = FovToRadius(g_aimbotFOV);
    ImVec2 bestPos{ 0.0f, 0.0f };
    bool found = false;

    constexpr int HEAD_BONE = 6;
    constexpr float PREDICT_TIME = 0.04f;

    for (int i = 1; i < entityCount; ++i) {
        if (entityLifeState[i] == 2 || entityHealth[i] <= 0)
            continue;
        if (g_teamCheck && entityTeam[i] == entityTeam[0])
            continue;

        Vector3 head;
        if (!GetBonePosition(i, HEAD_BONE, head))
            continue;

        Vector3 vel;
        ReadMem(entityPawnPtr[i] + cs2_dumper::offsets::client_dll::m_vecVelocity,
            &vel, sizeof(vel));
        head.x += vel.x * PREDICT_TIME;
        head.y += vel.y * PREDICT_TIME;
        head.z += vel.z * PREDICT_TIME;

        ImVec2 screen;
        if (!WorldToScreen(head, screen))
            continue;

        float dx = screen.x - cx;
        float dy = screen.y - cy;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < bestRadius) {
            bestRadius = dist;
            bestPos = screen;
            found = true;
        }
    }

    if (!found)
        return;

    // draw smaller white 'X'
    const float size = 4.0f;  // reduced from 6
    ImU32 col = IM_COL32(255, 255, 255, 255);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddLine(
        ImVec2(bestPos.x - size, bestPos.y - size),
        ImVec2(bestPos.x + size, bestPos.y + size),
        col, 1.5f);
    dl->AddLine(
        ImVec2(bestPos.x + size, bestPos.y - size),
        ImVec2(bestPos.x - size, bestPos.y + size),
        col, 1.5f);
}
