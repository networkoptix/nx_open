/*
�Copyright 05/07/2014 Intel Corporation All Rights Reserved.

The source code, information and material ("Material") contained herein is owned by Intel Corporation or 
its suppliers or licensors, and title to such Material remains with Intel Corporation or its suppliers or 
licensors. The Material contains proprietary information of Intel or its suppliers and licensors. The Material 
is protected by worldwide copyright laws and treaty provisions. No part of the Material may be used, copied, 
reproduced, modified, published, uploaded, posted, transmitted, distributed or disclosed in any way without 
Intel's prior express written permission. No license under any patent, copyright or other intellectual property 
rights in the Material is granted to or conferred upon you, either expressly, by implication, inducement, 
estoppel or otherwise. Any license under such intellectual property rights must be express and approved 
by Intel in writing.

This source file uses code snippets from Intel Media SDK sample code and has been modified. The license is 
available in the license folder. 


Unless otherwise agreed by Intel in writing, you may not remove or alter this notice or any other notice embedded 
in Materials by Intel or Intel�s suppliers or licensors in any way.�
*/

#ifdef _WIN32

#include <windowsx.h>
#include <stdio.h>
#include <system_error>

#include "GL/glew.h"
#include "GL/wglew.h"
#include "GL/gl.h"

#include "simpleDX9Device.h"

#include <nx/utils/log/log.h>

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        // Allow the window to be larger than the screen
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO *minmax = (MINMAXINFO *) lParam;
            minmax->ptMaxSize.x = 2000;
            minmax->ptMaxSize.y = 2000;
            minmax->ptMaxTrackSize.x = 2000;
            minmax->ptMaxTrackSize.y = 2000;
            return 0;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc (hWnd, message, wParam, lParam);
}

SimpleDXDevice::~SimpleDXDevice()
{
    if (m_pDevice)
        m_pDevice->Release();

    if (m_pD3d)
        m_pD3d->Release();

    if (m_pDeviceManager9)
        m_pDeviceManager9->Release();
}

HWND SimpleDXDevice::CreateDxWindow(int width, int height)
{
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = (HINSTANCE)GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"WindowClass";

    RegisterClassEx(&wc);

    HWND hWnd = CreateWindowEx(NULL, L"WindowClass", L"Shared Resource - DX",
        WS_OVERLAPPEDWINDOW, 0, 0, width, height,
        NULL, NULL, (HINSTANCE)GetModuleHandle(NULL), NULL);

    if (IsWindow(hWnd))
    {
        RECT rc = { 0, 0, width, height};

        DWORD dwStyle = GetWindowLongPtr(hWnd, GWL_STYLE);
        DWORD dwExStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
        AdjustWindowRectEx(&rc, dwStyle, FALSE, dwExStyle);
        SetWindowPos(hWnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOMOVE);
    }
    return hWnd;
    //POINT point = {0, 0};
    //return WindowFromPoint(point);
}

HRESULT SimpleDXDevice::CreateDevice(int width, int height, mfxU32 adapterNum)
{
    m_hWnd = CreateDxWindow(width, height);

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = m_hWnd;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;

    // A D3D9EX device is required to create the sharedSurface
    Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3d);

    // The interop definition states D3DCREATE_MULTITHREADED is required, but it may vary by vendor
    HRESULT hr = m_pD3d->CreateDeviceEx(adapterNum,
        D3DDEVTYPE_HAL,
        m_hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &d3dpp,
        NULL,
        &m_pDevice);

    // Since this demo only shows RGB->RGB conversion, verify that the hardware can do NV12->RGB conversion
    hr = m_pD3d->CheckDeviceFormatConversion(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        (D3DFORMAT) MAKEFOURCC('N', 'V', '1', '2'),
        D3DFMT_X8R8G8B8);
    hr = m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    hr = m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);


    hr = m_pDevice->ResetEx(&d3dpp, NULL);
    if (FAILED(hr)) return hr;

    hr = m_pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(hr)) return hr;

    UINT resetToken = 0;

    hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &m_pDeviceManager9);
    if (FAILED(hr)) return hr;

    hr = m_pDeviceManager9->ResetDevice(m_pDevice, resetToken);
    if (FAILED(hr)) return hr;

    if (!m_renderer.init(m_hWnd, m_pDevice, m_pD3d, width, height))
        return false;

    return S_OK;
}

#endif // _WIN32
