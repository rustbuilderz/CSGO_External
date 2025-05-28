#include <d3d11.h>
#include <tchar.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <thread>

#include "overlay.h"
#include "render.h"
#include "config.h"
#include "menu.h"
#include "memory.h"
#include <windows.h>
#include "aimbot.h"

int RunESP()
{
    // 1) Create overlay window
    WNDCLASSEX wc{ sizeof(wc), CS_CLASSDC, Overlay_WndProc, 0,0,
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
    if (!Overlay_CreateDeviceD3D(g_hWndOverlay)) return 1;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiStyle& style = ImGui::GetStyle();
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
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

    while (!g_shutdown && msg.message != WM_QUIT)
    {

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






        auto dl = ImGui::GetBackgroundDrawList();
        if (GetForegroundWindow() == hGameWnd) {
            for (int i = 1; i < entityCount; ++i) {
                if (g_showBoxes && g_showESP) RenderESPBox(dl, i);
                if (g_showHealthBar && g_showESP) RenderHealthBar(dl, i);
                if (g_showTeamText && g_showESP) RenderTeamText(dl, i);
                if (g_showSkeletons && g_showESP) DrawSkeleton(dl, i);
                if (g_showNames && g_showESP) DrawName(dl, i);
            }

        }
        if (g_drawFovCircle && g_enableAimbot) DrawFOVCircle();
        if (g_drawCrosshair) DrawCrosshair();
        if (g_showStats) RenderStatistics();
        DrawAimbotTargetMarker();








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

        g_pSwapChain->Present(0, 0);
        auto dt = std::chrono::steady_clock::now() - t0;
        if (dt < frameTime)
            std::this_thread::sleep_for(frameTime - dt);

    }

    // 5) Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    Overlay_CleanupDeviceD3D();
    DestroyWindow(g_hWndOverlay);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}

