#ifdef _WIN32

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtPlatformHeaders/QWGLNativeContext>


#include "renderer.h"

#include <nx/utils/log/log.h>

#define WGL_ACCESS_READ_ONLY_NV 0x0000

namespace nx::media::quick_sync::windows {

typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
typedef BOOL (WINAPI * PFNWGLDXCLOSEDEVICENVPROC) (HANDLE hDevice);
typedef BOOL (WINAPI * PFNWGLDXLOCKOBJECTSNVPROC) (HANDLE hDevice, GLint count, HANDLE* hObjects);
typedef HANDLE (WINAPI * PFNWGLDXOPENDEVICENVPROC) (void* dxDevice);
typedef HANDLE (WINAPI * PFNWGLDXREGISTEROBJECTNVPROC) (HANDLE hDevice, void* dxObject, GLuint name, GLenum type, GLenum access);
typedef BOOL (WINAPI * PFNWGLDXSETRESOURCESHAREHANDLENVPROC) (void* dxObject, HANDLE shareHandle);
typedef BOOL (WINAPI * PFNWGLDXUNLOCKOBJECTSNVPROC) (HANDLE hDevice, GLint count, HANDLE* hObjects);
typedef BOOL (WINAPI * PFNWGLDXUNREGISTEROBJECTNVPROC) (HANDLE hDevice, HANDLE hObject);

typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int* attribList);

bool convertToRgb(
    IDirect3DDevice9Ex *pDevice, mfxFrameSurface1* mfxSurface, IDirect3DSurface9* pSharedSurface)
{
    mfxHDLPair* pair = (mfxHDLPair*)mfxSurface->Data.MemId;
    IDirect3DSurface9* decodedSurface = (IDirect3DSurface9*)pair->first;
    // Copy the result to the shared surface
    RECT rc = { 0, 0, mfxSurface->Info.Width, mfxSurface->Info.Height};
    HRESULT hr = pDevice->BeginScene();
    if (FAILED(hr))
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to convert to RGB, begin scene, error code: %1",
            std::system_category().message(hr));
        return false;
    }

    hr = pDevice->StretchRect(decodedSurface, &rc, pSharedSurface, &rc, D3DTEXF_NONE);
    if (FAILED(hr))
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to convert to RGB, stretch rect, error code: %1",
                 std::system_category().message(hr));
        return false;
    }

    hr = pDevice->EndScene();
    if (FAILED(hr))
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to convert to RGB, end scene, error code: %1",
            std::system_category().message(hr));
        return false;
    }
    return true;
}

Renderer::~Renderer()
{
    close();
}

void Renderer::close()
{
    unregisterTexture();
    if (m_rc)
        wglDeleteContext(m_rc);
    if (m_dc)
        ::ReleaseDC(m_window, m_dc);
}

bool Renderer::initRenderSurface(IDirect3D9Ex* d3d)
{
    auto hr = m_device->GetRenderTarget(0, &m_renderTargetSurface);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to get render target, error code: %1",
            std::system_category().message(hr));
        return false;
    }

    D3DSURFACE_DESC rtDesc;
    m_renderTargetSurface->GetDesc(&rtDesc);

    hr = m_device->CreateOffscreenPlainSurface(rtDesc.Width,
        rtDesc.Height,
        rtDesc.Format,
        D3DPOOL_DEFAULT,
        &m_sharedSurface,
        &m_sharedSurfaceHandle);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to create offscreen plain surface, error code: %1",
            std::system_category().message(hr));
        return false;
    }
    hr = d3d->CheckDeviceFormatConversion(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        (D3DFORMAT) MAKEFOURCC('N', 'V', '1', '2'),
        D3DFMT_X8R8G8B8);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to CheckDeviceFormatConversion, error code: %1",
            std::system_category().message(hr));
        return false;
    }

    hr = m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to SetRenderState, error code: %1",
            std::system_category().message(hr));
        return false;
    }

    hr = m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to SetRenderState, error code: %1",
            std::system_category().message(hr));
        return false;
    }
    return true;
}

bool Renderer::init(
    HWND window, IDirect3DDevice9Ex* device, IDirect3D9Ex* d3d)
{
    m_device = device;
    m_window = window;

    if (!initRenderSurface(d3d))
        return false;

    static PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),  1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL,
        PFD_TYPE_RGBA,
        32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0,
        PFD_MAIN_PLANE,
        0, 0, 0, 0
    };

    m_dc = ::GetDC(m_window);
    GLuint PixelFormat = ChoosePixelFormat(m_dc, &pfd);
    if (!SetPixelFormat(m_dc, PixelFormat, &pfd))
    {
        NX_ERROR(this, "Failed to set pixel format for window %1, error: %2",
            m_window, GetLastError());
        return false;
    }

    m_rc = wglCreateContext(m_dc);
    if (!m_rc)
    {
        NX_ERROR(this, "Failed to create OpenGL context for window %1, error: %2",
            m_window, GetLastError());
        return false;
    }
    return true;
}

void Renderer::unregisterTexture()
{
    wglMakeCurrent(m_dc, m_rc);
    auto wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXUnregisterObjectNV");
    auto wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC)wglGetProcAddress("wglDXCloseDeviceNV");

    if (m_textureHandle)
    {
        wglDXUnregisterObjectNV(m_renderDeviceHandle, m_textureHandle);
        m_textureHandle = 0;
    }

    if (m_renderDeviceHandle)
    {
        wglDXCloseDeviceNV(m_renderDeviceHandle);
        m_renderDeviceHandle = 0;
    }
    wglMakeCurrent(NULL, NULL);
}

bool Renderer::registerTexture(GLuint textureId, QOpenGLContext* context)
{
    HGLRC guiGlrc = context->nativeHandle().value<QWGLNativeContext>().context();
    wglShareLists(guiGlrc, m_rc);
    wglMakeCurrent(m_dc, m_rc);

    auto wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC)wglGetProcAddress("wglDXOpenDeviceNV");
    auto wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXRegisterObjectNV");
    auto wglDXSetResourceShareHandleNV = (PFNWGLDXSETRESOURCESHAREHANDLENVPROC)wglGetProcAddress("wglDXSetResourceShareHandleNV");

    // Acquire a handle to the D3D device for use in OGL
    m_renderDeviceHandle = wglDXOpenDeviceNV(m_device);
    if (!m_renderDeviceHandle)
    {
        NX_ERROR(this, "Failed open device NV");
        return false;
    }
    // This registers a resource that was created as shared in DX with its shared handle
    BOOL success = wglDXSetResourceShareHandleNV(m_sharedSurface, m_sharedSurfaceHandle);

    m_textureHandle = wglDXRegisterObjectNV(
        m_renderDeviceHandle, m_sharedSurface, textureId, GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);
    if (m_textureHandle == NULL)
    {
        NX_ERROR(this, "Failed to register object NV, error code: %1", GetLastError());
        return false;
    }
    wglMakeCurrent(NULL, NULL);
    return true;
}

bool Renderer::render(
    mfxFrameSurface1* mfxSurface, bool isNewTexture, GLuint textureId, QOpenGLContext* context)
{
    if (isNewTexture || !m_textureHandle)
    {
        NX_DEBUG(this, "Register new texture: %1", textureId);
        unregisterTexture();
        if (!registerTexture(textureId, context))
            return false;
        NX_DEBUG(this, "Register new texture: %1 end", textureId);
    }
    if (!convertToRgb(m_device, mfxSurface, m_sharedSurface))
        return false;

    wglMakeCurrent(m_dc, m_rc);
    auto wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXLockObjectsNV");
    auto wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXUnlockObjectsNV");
    wglDXLockObjectsNV(m_renderDeviceHandle, 1, &m_textureHandle);
    wglDXUnlockObjectsNV(m_renderDeviceHandle, 1, &m_textureHandle);
    wglMakeCurrent(NULL, NULL);
    return true;
}

} // namespace nx::media::quick_sync::windows

#endif // _WIN32
