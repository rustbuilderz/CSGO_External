// esp.cpp
#include "menu.h"
#include "memory.h"
#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <chrono>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <thread>
#include <vector>
#include <map>
#include <string>

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
// ——— Globals ———
bool g_showESP = true;
bool g_showHealthBar = true;
bool g_showTeamText = true;
bool g_menuShown = false;
bool g_teamCheck = false;   // <— this is your one true definition

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


// D3D11 objects
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HWND                    g_hWndOverlay = nullptr;

// Forward
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI   WndProc(HWND, UINT, WPARAM, LPARAM);
bool   CreateDeviceD3D(HWND);
void   CleanupDeviceD3D();
void   CreateRenderTarget();
void   CleanupRenderTarget();

// World→Screen (unchanged)
static bool WorldToScreen(const Vector3& pos, ImVec2& out) {
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

static void DrawName(ImDrawList* dl, int entIdx,
    ImU32 color = IM_COL32(255, 255, 255, 255))
{
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

static void DrawSkeleton(ImDrawList* dl, int entIdx,
    ImU32 color = IM_COL32(255, 255, 255, 255),
    float thickness = 1.0f)
{
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

int RunESP()
{
    // 1) Create overlay window
    WNDCLASSEX wc{ sizeof(wc), CS_CLASSDC, WndProc, 0,0,
                   GetModuleHandle(NULL), NULL,NULL,NULL,NULL,
                   _T("CS2ESP"), NULL };
    RegisterClassEx(&wc);
    g_hWndOverlay = CreateWindowEx(
        WS_EX_TOPMOST
        | WS_EX_LAYERED
        | WS_EX_TRANSPARENT
        | WS_EX_TOOLWINDOW
        | WS_EX_NOACTIVATE,
        wc.lpszClassName, _T("CS2 External ESP"),
        WS_POPUP,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, wc.hInstance, NULL);
    SetLayeredWindowAttributes(g_hWndOverlay, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(g_hWndOverlay, SW_SHOW);
    UpdateWindow(g_hWndOverlay);

    // 2) Init D3D11 + ImGui
    if (!CreateDeviceD3D(g_hWndOverlay)) return 1;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hWndOverlay);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // 3) Find game window
    HWND hGameWnd = FindWindow(NULL, L"Counter-Strike 2");

    // 4) Main loop @60 FPS
    MSG msg; ZeroMemory(&msg, sizeof(msg));
    bool lastInsert = false;
    constexpr int   targetFPS = 60;
    const auto      frameTime = std::chrono::milliseconds(1000 / targetFPS);

    while (msg.message != WM_QUIT)
    {
        // — Pump messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // — Toggle menu on INSERT
        bool currInsert = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (currInsert && !lastInsert) {
            g_menuShown = !g_menuShown;
            LONG ex = GetWindowLong(g_hWndOverlay, GWL_EXSTYLE);
            if (g_menuShown) ex &= ~WS_EX_TRANSPARENT;
            else             ex |= WS_EX_TRANSPARENT;
            SetWindowLong(g_hWndOverlay, GWL_EXSTYLE, ex);
            SetLayeredWindowAttributes(g_hWndOverlay, RGB(0, 0, 0), 0, LWA_COLORKEY);
        }
        lastInsert = currInsert;

        auto t0 = std::chrono::steady_clock::now();

        // — Start ImGui frame
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();

        if (GetForegroundWindow() == hGameWnd && g_showESP) {
            // grab the local player's team from slot 0 once
            int localTeam = entityTeam[0];

            auto dl = ImGui::GetBackgroundDrawList();
            for (int i = 1; i < entityCount; ++i) {
                // skip dead, invalid, etc.
                DrawSkeleton(dl, i);
                DrawName(dl, i);

                if (entityLifeState[i] == 2 || entityHealth[i] <= 0)
                    continue;

                // NEW: if team-check flag is set and this entity is on your team, skip it
                if (g_teamCheck && entityTeam[i] == localTeam)
                    continue;



                ImVec2 foot, head;
                if (!WorldToScreen(entityPositions[i], foot))       continue;
                Vector3 hpos = entityPositions[i]; hpos.z += 75.0f;
                if (!WorldToScreen(hpos, head))                      continue;

                float h = foot.y - head.y; if (h <= 0)                continue;
                float w = h * 0.5f;
                ImVec2 p0(head.x - w * 0.5f, head.y), p1(p0.x + w, p0.y + h);
                dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 255), 0, 0, 1.5f);

                if (g_showHealthBar) {
                    float bw = 4;
                    ImVec2 hb0(p0.x - bw - 1, p0.y), hb1(p0.x - 1, p1.y);
                    dl->AddRectFilled(hb0, hb1, IM_COL32(0, 0, 0, 150));
                    float frac = entityHealth[i] / 100.f;
                    ImVec2 hf0(hb0.x, hb1.y - (hb1.y - hb0.y) * frac);
                    dl->AddRectFilled(hf0, hb1, IM_COL32(0, 255, 0, 255));
                }

                if (g_showTeamText) {
                    char buf[16];
                    sprintf_s(buf, "T:%d", entityTeam[i]);
                    dl->AddText(ImVec2(p1.x + 2, p0.y), IM_COL32(255, 255, 255, 255), buf);
                }
            }
        }

        // — Draw menu if shown
        if (g_menuShown && GetForegroundWindow() == hGameWnd) {
            RenderMenu();
        }

        // — Render ImGui
        ImGui::Render();
        const float clear[4] = { 0,0,0,0 };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // — Present + throttle
        g_pSwapChain->Present(1, 0);
        auto dt = std::chrono::steady_clock::now() - t0;
        if (dt < frameTime)
            std::this_thread::sleep_for(frameTime - dt);

    }

    // 5) Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hWndOverlay);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}

// — D3D11 Setup/Cleanup — 

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = { 60,1 };
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    if (D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice,
        &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();

    // Enable alpha blending
    D3D11_BLEND_DESC bd{}; auto& rt = bd.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_ZERO;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState* bs = nullptr;
    g_pd3dDevice->CreateBlendState(&bd, &bs);
    float bf[4] = { 0,0,0,0 };
    g_pd3dDeviceContext->OMSetBlendState(bs, bf, 0xFFFFFFFF);

    return true;
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release();           g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release();    g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();           g_pd3dDevice = nullptr; }
}

// — Win32 WndProc — 

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return TRUE;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) // disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
