#pragma once

#ifdef _WIN32

#include <d3d9.h>
#include <dxva2api.h>
#include <dxva.h>
#include <windows.h>
#include <atlbase.h>

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>

#include <mfx/mfxvideo++.h>

namespace nx::media::quick_sync::windows {

class Renderer
{
public:
    ~Renderer();

    bool init(HWND window, IDirect3DDevice9Ex* device, IDirect3D9Ex* d3d, int width, int height);
    bool render(
        mfxFrameSurface1* mfxSurface, bool isNewTexture, GLuint textureId, QOpenGLContext* context);

private:
    bool registerTexture(GLuint textureId, QOpenGLContext* context);
    bool initRenderSurface(IDirect3D9Ex* d3d);

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
};

} // namespace nx::media::quick_sync::windows

#endif // _WIN32


