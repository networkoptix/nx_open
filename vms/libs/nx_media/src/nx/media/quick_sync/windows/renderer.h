// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef _WIN32

#include <atlbase.h>
#include <d3d9.h>
#include <dxva.h>
#include <dxva2api.h>
#include <windows.h>

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>

#include <vpl/mfx.h>

namespace nx::media::quick_sync::windows {

class Renderer
{
public:
    ~Renderer();

    bool init(HWND window, IDirect3DDevice9Ex* device, IDirect3D9Ex* d3d);
    bool render(
        const mfxFrameSurface1* mfxSurface, bool isNewTexture, GLuint textureId, QOpenGLContext* context);
    void close();

private:
    bool registerTexture(GLuint textureId, QOpenGLContext* context);
    bool createSharedSurface(QSize size);
    bool initRenderSurface(IDirect3D9Ex* d3d);
    void unregisterTexture();

private:
    IDirect3DDevice9Ex* m_device = nullptr;
    CComPtr<IDirect3DSurface9> m_renderTargetSurface;
    CComPtr<IDirect3DSurface9> m_sharedSurface;
    HANDLE m_sharedSurfaceHandle = 0;

    HANDLE m_textureHandle = 0;
    HANDLE m_renderDeviceHandle = 0;
    HWND m_window = 0;
    HDC m_dc = 0;
    HGLRC m_rc = 0;

    QSize m_sharedSurfaceSize;
};

} // namespace nx::media::quick_sync::windows

#endif // _WIN32
