#include "screen_grabber.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "base/colorspace_convert/colorspace.h"
#include "base/log.h"

#include <Dwmapi.h>

QMutex CLScreenGrapper::m_instanceMutex;
int CLScreenGrapper::m_instanceCounter;


CLScreenGrapper::CLScreenGrapper(int displayNumber, int poolSize): 
    m_pD3D(0), 
    m_pd3dDevice(0), 
    m_displayNumber(displayNumber),
    m_frameNum(0),
    m_currentIndex(0)
{
    m_pSurface.resize(poolSize+1);
    memset(&m_rect, 0, sizeof(m_rect));

    {
        QMutexLocker locker(&m_instanceMutex);
        m_instanceCounter++;
        if (m_instanceCounter == 1)
            DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
    }
    m_initialized = InitD3D(GetDesktopWindow());
    m_timer.start();
}

CLScreenGrapper::~CLScreenGrapper()
{
    for (int i = 0; i < m_pSurface.size(); ++i)
    {
        if(m_pSurface[i])
        {
            m_pSurface[i]->Release();
            m_pSurface[i]=NULL;
        }
    }
    if(m_pd3dDevice)
    {
        m_pd3dDevice->Release();
        m_pd3dDevice=NULL;
    }
    if(m_pD3D)
    {
        m_pD3D->Release();
        m_pD3D=NULL;
    }
    QMutexLocker locker(&m_instanceMutex);
    m_instanceCounter--;
    if (m_instanceCounter == 0)
        DwmEnableComposition(DWM_EC_ENABLECOMPOSITION);
}

HRESULT	CLScreenGrapper::InitD3D(HWND hWnd)
{
    D3DPRESENT_PARAMETERS	d3dpp;

    if((m_pD3D=Direct3DCreate9(D3D_SDK_VERSION))==NULL)
    {
        qWarning() << "Unable to Create Direct3D ";
        return E_FAIL;
    }

    if(FAILED(m_pD3D->GetAdapterDisplayMode(m_displayNumber,&m_ddm)))
    {
        qWarning() << "Unable to Get Adapter Display Mode";
        return E_FAIL;
    }

    ZeroMemory(&d3dpp,sizeof(D3DPRESENT_PARAMETERS));

    d3dpp.Windowed=true; // not fullscreen
    d3dpp.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    d3dpp.BackBufferFormat = m_ddm.Format;
    d3dpp.BackBufferHeight = m_rect.bottom = m_ddm.Height;
    d3dpp.BackBufferWidth = m_rect.right = m_ddm.Width;
    d3dpp.MultiSampleType=D3DMULTISAMPLE_NONE;
    d3dpp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow=hWnd;
    d3dpp.PresentationInterval=D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp.FullScreen_RefreshRateInHz=D3DPRESENT_RATE_DEFAULT;

    if(FAILED(m_pD3D->CreateDevice(m_displayNumber, D3DDEVTYPE_HAL,hWnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING ,&d3dpp,&m_pd3dDevice)))
    {
        qWarning() << "Unable to Create Device";
        return E_FAIL;
    }
    for (int i = 0; i < m_pSurface.size(); ++i)
    {
        if(FAILED(m_pd3dDevice->CreateOffscreenPlainSurface(m_ddm.Width, m_ddm.Height, D3DFMT_A8R8G8B8, 
            D3DPOOL_SCRATCH, 
            &m_pSurface[i], NULL)))
        {
            qWarning() << "Unable to Create Surface";
            return E_FAIL;
        }
    }
    
    return S_OK;
};

IDirect3DSurface9* CLScreenGrapper::captureFrame()
{
    if (!m_pd3dDevice)
        return false;

    QTime t4;
    t4.start();
    DWORD hr;
    if(FAILED(hr = m_pd3dDevice->GetFrontBufferData(0, m_pSurface[m_currentIndex])))
    {
        cl_log.log("Unable to capture frame", cl_logWARNING);
    }
    IDirect3DSurface9* rez = m_pSurface[m_currentIndex];
    m_currentIndex = m_currentIndex < m_pSurface.size()-1 ? m_currentIndex+1 : 0;
    cl_log.log("capture time=", t4.elapsed(),cl_logALWAYS);
    return rez;
}

bool CLScreenGrapper::SurfaceToFrame(IDirect3DSurface9* pSurface, AVFrame* pFrame)
{
    D3DLOCKED_RECT	lockedRect;

    if(FAILED(pSurface->LockRect(&lockedRect, 0, D3DLOCK_NO_DIRTY_UPDATE|D3DLOCK_NOSYSLOCK|D3DLOCK_READONLY)))
    {
        qWarning() << "Unable to Lock Front Buffer Surface";
        return false;
    }

    if (pFrame->width == 0)
    {
        int videoBufSize = avpicture_get_size(format(), width(), height());
        quint8* videoBuf = (quint8*) av_malloc(videoBufSize);

        avpicture_fill((AVPicture *)pFrame, videoBuf, format(), width(), height());
        pFrame->key_frame = 1;
        pFrame->pict_type = AV_PICTURE_TYPE_I;
        pFrame->width = m_ddm.Width;
        pFrame->height = m_ddm.Height;
        pFrame->format = format();
        pFrame->sample_aspect_ratio.num = 1;
        pFrame->sample_aspect_ratio.den = 1;

    }
    pFrame->coded_picture_number = pFrame->coded_picture_number = m_frameNum++;
    pFrame->pts = m_timer.elapsed();
    pFrame->best_effort_timestamp = pFrame->pts;

#if 1
    bgra_to_yv12_mmx((unsigned char*)lockedRect.pBits, m_ddm.Width*4, pFrame->data[0], pFrame->data[1], pFrame->data[2], 
        pFrame->linesize[0], pFrame->linesize[1], m_ddm.Width, m_ddm.Height, 0);
#else
    memcpy(pFrame->data[0], lockedRect.pBits, m_ddm.Width*4 * m_ddm.Height);
#endif
    pSurface->UnlockRect();
}
