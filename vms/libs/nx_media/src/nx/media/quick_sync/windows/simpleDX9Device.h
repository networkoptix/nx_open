/*
“Copyright 05/07/2014 Intel Corporation All Rights Reserved.

The source code, information and material ("Material") contained herein is owned by Intel Corporation or 
its suppliers or licensors, and title to such Material remains with Intel Corporation or its suppliers or 
licensors. The Material contains proprietary information of Intel or its suppliers and licensors. The Material 
is protected by worldwide copyright laws and treaty provisions. No part of the Material may be used, copied, 
reproduced, modified, published, uploaded, posted, transmitted, distributed or disclosed in any way without 
Intel's prior express written permission. No license under any patent, copyright or other intellectual property 
rights in the Material is granted to or conferred upon you, either expressly, by implication, inducement, 
estoppel or otherwise. Any license under such intellectual property rights must be express and approved 
by Intel in writing.

Unless otherwise agreed by Intel in writing, you may not remove or alter this notice or any other notice embedded 
in Materials by Intel or Intel’s suppliers or licensors in any way.”
*/
#pragma once

#ifdef _WIN32

#include <d3d9.h>
#include <dxva2api.h>
#include <dxva.h>
#include <windows.h>

#include <mfx/mfxvideo++.h>

class SimpleDXDevice
{
public:
    SimpleDXDevice();
    ~SimpleDXDevice();

    HRESULT CreateDevice(DWORD dwWidth, DWORD dwHeight, mfxU32 adapterNum);
    IDirect3DDeviceManager9* GetDeviceManager9() { return m_pDeviceManager9; }
    IDirect3DDevice9Ex* GetDevice() { return m_pDevice;}
    IDirect3DSurface9* GetSharedSurface() { return m_pSharedSurface; }
    HANDLE GetSharedHandle() { return m_hSharedSurface; }
    HWND getWindow() { return m_hWnd; }

protected:
    HWND SimpleDXDevice::CreateDxWindow(DWORD dwWidth, DWORD dwHeight);

private:
    IDirect3D9Ex* m_pD3d;
    IDirect3DDevice9Ex* m_pDevice;
    IDirect3DSurface9* m_pRenderTargetSurface;
    IDirect3DSurface9* m_pSharedSurface;
    HANDLE m_hSharedSurface;
    IDirect3DDeviceManager9* m_pDeviceManager9;

    HWND m_hWnd;
};

#endif // _WIN32
