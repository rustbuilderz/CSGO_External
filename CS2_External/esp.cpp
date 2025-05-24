// esp.cpp
#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <cstdio>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "memory.h"

// D3D globals
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI   WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool             CreateDeviceD3D(HWND hWnd);
void             CleanupDeviceD3D();
void             CreateRenderTarget();
void             CleanupRenderTarget();

// Convert world coordinates to screen coordinates
static bool WorldToScreen(const Vector3& pos, ImVec2& out) {
    float clipX = pos.x * viewMatrix[0] + pos.y * viewMatrix[1] + pos.z * viewMatrix[2] + viewMatrix[3];
    float clipY = pos.x * viewMatrix[4] + pos.y * viewMatrix[5] + pos.z * viewMatrix[6] + viewMatrix[7];
    float w = pos.x * viewMatrix[12] + pos.y * viewMatrix[13] + pos.z * viewMatrix[14] + viewMatrix[15];
    if (w < 0.01f) return false;
    float invW = 1.0f / w;
    float ndcX = clipX * invW;
    float ndcY = clipY * invW;

    ImGuiIO& io = ImGui::GetIO();
    out.x = (ndcX * 0.5f + 0.5f) * io.DisplaySize.x;
    out.y = (1.0f - (ndcY * 0.5f + 0.5f)) * io.DisplaySize.y;
    return true;
}

// esp.cpp — the full RunESP() function
int RunESP() {
    WNDCLASSEX wc{ sizeof(wc), CS_CLASSDC, WndProc, 0, 0,
                   GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                   _T("CS2ESP"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT,
        wc.lpszClassName, _T("CS2 External ESP"),
        WS_POPUP, 0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, wc.hInstance, NULL);
    SetLayeredWindowAttributes(hwnd,0,0,LWA_COLORKEY);
    SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    MSG msg; ZeroMemory(&msg,sizeof(msg));
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        auto dl = ImGui::GetBackgroundDrawList();
        for (int i = 0; i < entityCount; ++i) {
            if (i == 0) continue;                   // skip self
            if (entityLifeState[i] == 2 || entityHealth[i] <= 0) continue;

            ImVec2 p0(entityHeadPos[i].x - (entityFootPos[i].y-entityHeadPos[i].y)*0.25f,
                      entityHeadPos[i].y);
            ImVec2 p1(entityHeadPos[i].x + (entityFootPos[i].y-entityHeadPos[i].y)*0.25f,
                      entityFootPos[i].y);
            dl->AddRect(p0,p1,IM_COL32(255,0,0,255),0,0,2);

            // health bar
            float bw = 4.0f;
            ImVec2 hb0(p0.x - bw -1, p0.y);
            ImVec2 hb1(p0.x -1,      p1.y);
            dl->AddRectFilled(hb0,hb1,IM_COL32(0,0,0,150));
            float frac = entityHealth[i]/100.f;
            ImVec2 hf0(hb0.x, hb1.y - (hb1.y-hb0.y)*frac);
            ImVec2 hf1(hb1.x, hb1.y);
            dl->AddRectFilled(hf0,hf1,IM_COL32(0,255,0,255));

            // team text
            char buf[16];
            sprintf_s(buf, sizeof(buf), "T:%d", entityTeam[i]);
            dl->AddText(ImVec2(p1.x+2,p0.y), IM_COL32(255,255,255,255), buf);
        }

        ImGui::Render();
        static float clear[4]={0,0,0,0};
        g_pd3dDeviceContext->OMSetRenderTargets(1,&g_mainRenderTargetView,NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView,clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(0,0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}


bool CreateDeviceD3D(HWND hWnd) {
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

    D3D11_BLEND_DESC blendDesc{};
    auto& rt = blendDesc.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_ZERO;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState* bs = nullptr;
    g_pd3dDevice->CreateBlendState(&blendDesc, &bs);
    float bf[4] = { 0,0,0,0 };
    g_pd3dDeviceContext->OMSetBlendState(bs, bf, 0xFFFFFFFF);
    return true;
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* bb = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&bb));
    g_pd3dDevice->CreateRenderTargetView(bb, nullptr, &g_mainRenderTargetView);
    bb->Release();
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
