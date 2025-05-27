#include "overlay.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <windows.h>
#include <psapi.h>
#include "ImGui/imgui.h"
HWND                    g_hWndOverlay = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;


LRESULT WINAPI Overlay_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return TRUE;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            Overlay_CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            Overlay_CreateRenderTarget();
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


void Overlay_CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

bool Overlay_CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = { 60,1 };
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 4;
    sd.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    if (D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice,
        &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    Overlay_CreateRenderTarget();

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

void Overlay_CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void Overlay_CleanupDeviceD3D()
{
    Overlay_CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release();           g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release();    g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();           g_pd3dDevice = nullptr; }
}


// one‐time init for CPU usage
static bool      s_statsInit = false;
static int       s_numProcs = 1;
static ULONGLONG s_lastTime = 0;
static ULONGLONG s_lastCPU = 0;
static HANDLE    s_hProcess = nullptr;

static void InitStats()
{
    SYSTEM_INFO si;    GetSystemInfo(&si);
    s_numProcs = si.dwNumberOfProcessors;
    s_hProcess = GetCurrentProcess();

    FILETIME ftNow, ftCreate, ftExit, ftKernel, ftUser;
    GetSystemTimeAsFileTime(&ftNow);
    s_lastTime = (((ULONGLONG)ftNow.dwHighDateTime) << 32) | ftNow.dwLowDateTime;

    GetProcessTimes(s_hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser);
    s_lastCPU = ((((ULONGLONG)ftKernel.dwHighDateTime) << 32) | ftKernel.dwLowDateTime)
        + ((((ULONGLONG)ftUser.dwHighDateTime) << 32) | ftUser.dwLowDateTime);

    s_statsInit = true;
}

void RenderStatistics()
{
    if (!s_statsInit) InitStats();

    // 1) FPS & ms
    float fps = ImGui::GetIO().Framerate;
    float ms = fps > 0.0f ? 1000.0f / fps : 0.0f;

    // 2) CPU%
    FILETIME ftNow, ftCreate, ftExit, ftKernel, ftUser;
    GetSystemTimeAsFileTime(&ftNow);
    ULONGLONG now = (((ULONGLONG)ftNow.dwHighDateTime) << 32) | ftNow.dwLowDateTime;

    GetProcessTimes(s_hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser);
    ULONGLONG curCPU = ((((ULONGLONG)ftKernel.dwHighDateTime) << 32) | ftKernel.dwLowDateTime)
        + ((((ULONGLONG)ftUser.dwHighDateTime) << 32) | ftUser.dwLowDateTime);

    ULONGLONG diffCPU = curCPU - s_lastCPU;
    ULONGLONG diffTime = now - s_lastTime;
    float cpuPercent = diffTime > 0
        ? (diffCPU / (float)diffTime) / s_numProcs * 100.0f
        : 0.0f;

    s_lastTime = now;
    s_lastCPU = curCPU;

    // 3) RAM (working set)
    PROCESS_MEMORY_COUNTERS pmc{};
    GetProcessMemoryInfo(s_hProcess, &pmc, sizeof(pmc));
    float memMB = pmc.WorkingSetSize / (1024.0f * 1024.0f);

    // 4) Draw a little window
    ImGui::Begin("Stats", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("FPS: %.1f (%.3f ms)", fps, ms);
    ImGui::Text("CPU: %.1f%%", cpuPercent);
    ImGui::Text("RAM: %.1f MB", memMB);
    ImGui::End();
}