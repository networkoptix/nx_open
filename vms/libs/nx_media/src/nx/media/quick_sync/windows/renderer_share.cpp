
#ifdef _WIN32

#include "GL/glew.h"
#include "GL/wglew.h"
#include "GL/gl.h"

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtPlatformHeaders/QWGLNativeContext>


#include "renderer_share.h"

#include <nx/utils/log/log.h>

namespace nx::media::quick_sync::windows {

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

RendererShare::~RendererShare()
{
    if (m_sharedSurface)
        m_sharedSurface->Release();

    if (m_renderTargetSurface)
        m_renderTargetSurface->Release();

     // TODO close all
}

bool RendererShare::initRenderSurface()
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
    hr = m_d3d->CheckDeviceFormatConversion(
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

bool RendererShare::init(HWND window, IDirect3DDevice9Ex* device, IDirect3D9Ex* d3d, int width, int height)
{
//  auto wglSwapIntervalEXT = (WGL_SWAP_INTERVAL_EXT)wglGetProcAddress("wglSwapIntervalEXT");
//  auto wglDXOpenDeviceNV = (WGL_DX_OPEN_DEVICE_NV)wglGetProcAddress("wglDXOpenDeviceNV");
//  auto wglDXRegisterObjectNV = (WGL_DX_REGISTER_OBJECT_NV)wglGetProcAddress("wglDXRegisterObjectNV");
//  auto wglDXSetResourceShareHandleNV =
//      (WGL_DX_SET_RESOURCE_SHARE_HANDLE_NV)wglGetProcAddress("wglDXSetResourceShareHandleNV");

    m_device = device;
    m_window = window;
    m_d3d = d3d;
    m_width = width;
    m_height = height;

    if (!initRenderSurface())
        return false;

    static PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),  1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0,
        PFD_MAIN_PLANE,
        0, 0, 0, 0
    };

    m_dc = ::GetDC(m_window);
    GLuint PixelFormat = ChoosePixelFormat(m_dc, &pfd);
    SetPixelFormat(m_dc, PixelFormat, &pfd);
    m_rc = wglCreateContext(m_dc);

    wglMakeCurrent(m_dc, m_rc);

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        NX_DEBUG(this, "Failed init glew: %1", glewGetErrorString(err));
        return false;
    }
    wglSwapIntervalEXT(0);

    wglMakeCurrent(nullptr, nullptr);
    return true;
}

bool RendererShare::init(GLuint textureId, QOpenGLContext* context)
{
#if 1
    HGLRC guiGlrc = context->nativeHandle().value<QWGLNativeContext>().context();
    wglShareLists(guiGlrc, m_rc);
    wglMakeCurrent(m_dc, m_rc);
#else
    m_context.setShareContext(QOpenGLContext::globalShareContext());
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    m_context.setFormat(format);
    m_offscreenSurface.setFormat(m_context.format());
    m_offscreenSurface.create();
    m_context.makeCurrent(&m_offscreenSurface);
#endif

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

bool RendererShare::render(mfxFrameSurface1* mfxSurface, bool isNewTexture, GLuint textureId, QOpenGLContext* context)
{
    if (isNewTexture)
    {
        if (!init(textureId, context))
            return false;
    }
    if (!convertToRgb(m_device, mfxSurface, m_sharedSurface))
        return false;


    wglMakeCurrent(m_dc, m_rc);
    wglDXLockObjectsNV(m_renderDeviceHandle, 1, &m_textureHandle);
    wglDXUnlockObjectsNV(m_renderDeviceHandle, 1, &m_textureHandle);
    wglMakeCurrent(NULL, NULL);

    return true;
}

} // namespace nx::media::quick_sync::windows

#endif // _WIN32

