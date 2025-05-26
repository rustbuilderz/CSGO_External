#pragma once

#include <windows.h>
#include <d3d11.h>

// these live in overlay.cpp
extern HWND                    g_hWndOverlay;
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern IDXGISwapChain* g_pSwapChain;
extern ID3D11RenderTargetView* g_mainRenderTargetView;

// Win32 window proc
LRESULT WINAPI Overlay_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
// forward ImGui Win32 message handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// D3D setup/teardown
bool  Overlay_CreateDeviceD3D(HWND hWnd);
void  Overlay_CreateRenderTarget();
void  Overlay_CleanupRenderTarget();
void  Overlay_CleanupDeviceD3D();
void RenderStatistics();
