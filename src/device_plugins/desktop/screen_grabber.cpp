#include "screen_grabber.h"

#ifdef Q_OS_WIN

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "base/colorspace_convert/colorspace.h"
#include "base/log.h"

#include <Dwmapi.h>

QMutex CLScreenGrapper::m_instanceMutex;
int CLScreenGrapper::m_aeroInstanceCounter;


CLScreenGrapper::CLScreenGrapper(int displayNumber, int poolSize, CaptureMode mode, bool captureCursor): 
    m_pD3D(0), 
    m_pd3dDevice(0), 
    m_displayNumber(displayNumber),
    m_frameNum(0),
    m_currentIndex(0),
    m_mode(mode),
    m_poolSize(poolSize+1),
    m_captureCursor(captureCursor)
{
    memset(&m_rect, 0, sizeof(m_rect));

    if (m_mode == CaptureMode_DesktopWithoutAero)
    {
        QMutexLocker locker(&m_instanceMutex);
        if (++m_aeroInstanceCounter == 1)
            DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
    }
    m_initialized = InitD3D(GetDesktopWindow());
    m_cursorDC = CreateCompatibleDC(0);
    m_timer.start();
}

CLScreenGrapper::~CLScreenGrapper()
{
    for (int i = 0; i < m_openGLData.size(); ++i)
        delete [] m_openGLData[i];

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
    if (m_mode == CaptureMode_DesktopWithoutAero)
    {
        QMutexLocker locker(&m_instanceMutex);
        if (--m_aeroInstanceCounter == 0)
            DwmEnableComposition(DWM_EC_ENABLECOMPOSITION);
    }
    DeleteDC(m_cursorDC);
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


    if (m_mode != CaptureMode_Application)
    {
        if(FAILED(m_pD3D->CreateDevice(m_displayNumber, D3DDEVTYPE_HAL,hWnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING ,&d3dpp,&m_pd3dDevice)))
        {
            qWarning() << "Unable to Create Device";
            return E_FAIL;
        }
        m_pSurface.resize(m_poolSize);
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
    }
    else
    {
        for (int i = 0; i < m_poolSize; ++i)
            m_openGLData << new quint8[m_rect.right * m_rect.bottom * 4];
    }
    return S_OK;
};


void CLScreenGrapper::captureFrameOpenGL(void* data)
{
    glReadBuffer(GL_FRONT); 
    glReadPixels(0, 0, m_rect.right, m_rect.bottom, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);
    if (m_captureCursor)
        drawCursor((quint32*) data, m_ddm.Width);
}

void* CLScreenGrapper::captureFrame()
{
    void* rez = 0;

    if (m_mode == CaptureMode_Application) 
    {   
        rez = m_openGLData[m_currentIndex];
        //captureFrameOpenGL(rez);
        QGenericReturnArgument ret;
        QMetaObject::invokeMethod(this, "captureFrameOpenGL", Qt::BlockingQueuedConnection, ret, Q_ARG(void*, rez));
    }
    else 
    {   // direct3D capture mode
        if (!m_pd3dDevice)
            return false;
        if(FAILED(m_pd3dDevice->GetFrontBufferData(0, m_pSurface[m_currentIndex])))
            cl_log.log("Unable to capture frame", cl_logWARNING);
        if (m_captureCursor)
        {
            D3DLOCKED_RECT	lockedRect;
            if(!FAILED(m_pSurface[m_currentIndex]->LockRect(&lockedRect, 0, D3DLOCK_NO_DIRTY_UPDATE|D3DLOCK_NOSYSLOCK|D3DLOCK_READONLY)))
            {
                drawCursor((quint32*) lockedRect.pBits, m_ddm.Width);
                m_pSurface[m_currentIndex]->UnlockRect();
            }
        }
        rez = m_pSurface[m_currentIndex];
    }
    m_currentIndex = m_currentIndex < m_pSurface.size()-1 ? m_currentIndex+1 : 0;
    return rez;
}

void bgra_to_yv12_sse(quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, int yStride, int uvStride, int width, int height)
{
    static const quint16 __declspec(align(16)) sse_2000[]         = { 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020 };
    static const quint16 __declspec(align(16)) sse_00a0[]         = { 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210 };
    static const quint32 __declspec(align(16)) sse_mask_color[]   = { 0x00003fc0, 0x00003fc0, 0x00003fc0, 0x00003fc0};
    static const qint16 __declspec(align(16)) sse_01[] = {0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001};

    static const int K = 32768;
    static const qint16 __declspec(align(16)) y_r_coeff[] =  { 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K };
    static const qint16 __declspec(align(16)) y_g_coeff[] =  { 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K };
    static const qint16 __declspec(align(16)) y_b_coeff[] =  { 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K };

    static const qint16 __declspec(align(16)) uv_r_coeff[] = { 0.439*K,  0.439*K,  0.439*K,  0.439*K,    -0.148*K, -0.148*K, -0.148*K, -0.148*K };
    static const qint16 __declspec(align(16)) uv_g_coeff[] = {-0.368*K, -0.368*K, -0.368*K, -0.368*K,    -0.291*K, -0.291*K, -0.291*K, -0.291*K };
    static const qint16 __declspec(align(16)) uv_b_coeff[] = {-0.071*K, -0.071*K, -0.071*K, -0.071*K,     0.439*K,  0.439*K,  0.439*K,  0.439*K };

    height /= 2;
    int xCount = width  / 8;

    int rgbaPostOffset = xStride - xCount*32;
    int yPostOffset = yStride - width;
    int uvPostOffset = uvStride - width/2;

    // EAX - u
    // ECX - v
    // edx - RGBA stride in bytes
    // ebx = Y stride
    // esi - source RGB
    // edi - Y
    __asm 
    {
        mov esi, rgba
        mov edi, y
        mov eax, u
        mov edx, xStride
        mov ebx, yStride
        mov ecx, v
loop1:
        PREFETCHNTA [esi + 64]
        PREFETCHNTA [esi+edx + 64]

        // prepare first line
        MOVAPS    xmm0, [esi] // BGRA
        MOVAPS    xmm6, [esi+16] // BGRA

        MOVAPS    xmm1, [esi]
        MOVAPS    xmm7, [esi+16]

        psllD xmm0, 6 // R
        psllD xmm6, 6 // R

        psrlD xmm1, 2 // G
        psrlD xmm7, 2 // G

        pand xmm0, sse_mask_color
        pand xmm6, sse_mask_color

        pand xmm1, sse_mask_color
        pand xmm7, sse_mask_color

        packusdw xmm0,xmm6 // pack dword to word
        packusdw xmm1,xmm7

        MOVAPS    xmm5, [esi]
        MOVAPS    xmm6, [esi+16]

        MOVAPS    xmm3, [esi+edx] // BGRA
        MOVAPS    xmm7, [esi+edx+16] // BGRA

        psrlD xmm5, 10 // B
        psrlD xmm6, 10 // B

        psrlD xmm3, 2 // G
        psrlD xmm7, 2 // G

        pand xmm5, sse_mask_color
        pand xmm6, sse_mask_color

        pand xmm3, sse_mask_color
        pand xmm7, sse_mask_color

        packusdw xmm5,xmm6
        packusdw xmm3,xmm7

        // prepare second line
        // -------------------------------------------------------------------------------------------

        // 2,5 - for B
        MOVAPS    xmm2, [esi+edx]
        MOVAPS    xmm6, [esi+edx+16]

        MOVAPS    xmm4, [esi+edx]
        MOVAPS    xmm7, [esi+edx+16]

        psllD xmm2, 6 // R
        psllD xmm6, 6 // R

        psrlD xmm4, 10 // B
        psrlD xmm7, 10 // B

        pand xmm2, sse_mask_color
        pand xmm6, sse_mask_color

        pand xmm4, sse_mask_color
        pand xmm7, sse_mask_color

        packusdw xmm2,xmm6 // pack dword to word
        packusdw xmm4,xmm7

        // ---------------------------------------------------------------------------------------

        MOVAPS xmm6, xmm0
        MOVAPS xmm7, xmm1

        // 0,1,5 - first line. 2,3,4 - second

        // calculate Y of first line
        pmulhw xmm6, y_b_coeff
        pmulhw xmm7, y_g_coeff

        pavgw xmm0, xmm2
        paddw xmm6, xmm7

        MOVAPS xmm7, xmm5

        paddw xmm6, sse_00a0
        pmulhw xmm7, y_r_coeff

        pavgw xmm1, xmm3
        paddw xmm6, xmm7

        pavgw xmm5, xmm4
        psrlw xmm6, 5

        // ----------------------------------------------

        MOVAPS xmm7, xmm3
        packuswb xmm6, xmm6

        pmulhw xmm7, y_g_coeff
        MOVLPS [edi], xmm6

        // calculate Y of second line
        MOVAPS xmm6, xmm2
        PMADDWD xmm1, sse_01

        pmulhw xmm6, y_b_coeff
        PMADDWD xmm0, sse_01

        paddw xmm6, xmm7
        MOVAPS xmm7, xmm4
                        
        PMADDWD xmm5, sse_01
        pmulhw xmm7, y_r_coeff

        paddw xmm6, sse_00a0
        packusdw xmm0, xmm0

        paddw xmm6, xmm7
        packusdw xmm1, xmm1

        psrlw xmm6, 5
        packusdw xmm5, xmm5

        pmulhw xmm0, uv_b_coeff
        packuswb xmm6, xmm6

        pmulhw xmm1, uv_g_coeff
        MOVLPS [edi+ebx], xmm6

        // ---------------------------------------------------------
        // calculate UV colors


        paddw xmm0, sse_2000
        pmulhw xmm5, uv_r_coeff

        paddw xmm0, xmm1
        add esi, 32

        paddw xmm0, xmm5
        add edi, 8

        psrlw xmm0, 6
        packuswb xmm0, xmm0

        MOVD [ecx], xmm0
        psrlq xmm0, 32

        add ecx, 4
        MOVD [eax], xmm0
        
        add eax, 4
        dec xCount
        jnz loop1

        // ------------------ Y loop
        push width
        pop xCount
        shr xCount, 3

        add esi, edx
        add edi, ebx

        add esi, rgbaPostOffset
        add edi, yPostOffset

        add eax, uvPostOffset
        add ecx, uvPostOffset

        dec height
        jnz loop1
    }
}

void bgra_yuv420(quint8* rgba, quint8* yptr, quint8* uptr, quint8* vptr, int width, int height)
{
    int xStride = width*4;
    quint8* yptr2 = yptr + width;
    quint8* rgba2 = rgba + xStride;
    for (int y = height/2; y > 0; --y)
    {
        for (int x = width/2; x > 0; --x)
        {
            // process 4 Y pixels
            quint8 y1 = rgba[0]*0.098f + rgba[1]*0.504f + rgba[2]*0.257f + 16.5f;
            *yptr++ = y1;
            quint8 y2 = rgba[4]*0.098f + rgba[5]*0.504f + rgba[6]*0.257f + 16.5f;
            *yptr++ = y2;

            quint8 y3 = rgba2[0]*0.098 + rgba2[1]*0.504 + rgba2[2]*0.257 + 16.5f;
            *yptr2++ = y3;
            quint8 y4 = rgba2[4]*0.098 + rgba2[5]*0.504 + rgba2[6]*0.257 + 16.5f;
            *yptr2++ = y4;
            
            // calculate avarage RGB values for UV pixels
            quint8 avgB = (rgba[0]+rgba[4]+rgba2[0]+rgba2[4]+2) >> 2;
            quint8 avgG = (rgba[1]+rgba[5]+rgba2[1]+rgba2[5]+2) >> 2;
            quint8 avgR = (rgba[2]+rgba[6]+rgba2[2]+rgba2[6]+2) >> 2;

            // write UV pixels
            *vptr++ = 0.439f*avgR  - 0.368f*avgG - 0.071f*avgB + 128.5f;
            *uptr++ = -0.148f*avgR - 0.291f*avgG + 0.439f*avgB + 128.5f;

            rgba += 8;
            rgba2 += 8;
        }
        rgba   += xStride;
        rgba2  += xStride;
        yptr  += width;
        yptr2 += width;
    }
}

static inline void andPixel(quint32* data, int stride, int x, int y, quint32* srcPixel)
{
    data[y*stride+x] &= *srcPixel;
}

static inline void xorPixel(quint32* data, int stride, int x, int y, quint32* srcPixel)
{
    data[y*stride+x] ^= *srcPixel;
}

void CLScreenGrapper::drawCursor(quint32* data, int dataStride) const
{
    static const int MAX_CURSOR_SIZE = 64;
    CURSORINFO  pci;
    pci.cbSize = sizeof(CURSORINFO);
    if (GetCursorInfo(&pci))
    {
        if (! (pci.flags & CURSOR_SHOWING))
            return;

        ICONINFO icInfo;
        memset(&icInfo, 0, sizeof(ICONINFO));
        if (GetIconInfo(pci.hCursor, &icInfo))
        {
            int xPos = pci.ptScreenPos.x - ((int)icInfo.xHotspot);
            int yPos = pci.ptScreenPos.y - ((int)icInfo.yHotspot);

            quint8 maskBits[MAX_CURSOR_SIZE*MAX_CURSOR_SIZE*2/8 * 16];
            quint8 colorBits[MAX_CURSOR_SIZE*MAX_CURSOR_SIZE*4]; // max cursor size: 64x64
            quint8 lpbiData[sizeof(BITMAPINFO) + 4]; // 2 additional colors
            quint8 lpbiColorData[sizeof(BITMAPINFO) + 3*4]; // 4 additional colors

            BITMAPINFO* lpbi = (BITMAPINFO*) &lpbiData;
            BITMAPINFO* lpbiColor = (BITMAPINFO*) &lpbiColorData;
            memset(lpbi, 0, sizeof(BITMAPINFO));
            memset(lpbiColor, 0, sizeof(BITMAPINFO));
            lpbi->bmiHeader.biSize = sizeof(BITMAPINFO);
            lpbiColor->bmiHeader.biSize = sizeof(BITMAPINFO);
            if (GetDIBits(m_cursorDC, icInfo.hbmMask, 0, 0, 0, lpbi, 0) == 0)
            {
                DeleteObject(icInfo.hbmMask);
                DeleteObject(icInfo.hbmColor);
                return;
            }
            if (lpbi->bmiHeader.biClrUsed > 2 || GetDIBits(m_cursorDC, icInfo.hbmMask, 0, lpbi->bmiHeader.biHeight, maskBits, lpbi, 0) == 0)
            {
                DeleteObject(icInfo.hbmMask);
                DeleteObject(icInfo.hbmColor);
                return;
            }


            quint8* maskBitsPtr = maskBits;
            int bitmaskStride = lpbi->bmiHeader.biSizeImage / lpbi->bmiHeader.biHeight;

            // and mask

            quint8 maskFlag = icInfo.hbmColor ? 0 : 0x80;

            int cursorHeight = lpbi->bmiHeader.biHeight;
            if (icInfo.hbmColor == 0)
                cursorHeight /= 2;

            for (int y = yPos + cursorHeight-1; y >= yPos; --y) 
            {
                for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                {
                    quint8 mask = *maskBitsPtr++;
                    for (int k = 0; k < 8; ++k)
                    {
                        if ((mask & 0x80) == maskFlag && x > 0 && y > 0 && x < m_ddm.Width && y < m_ddm.Height)
                            andPixel(data, dataStride, x, y, (quint32*) &lpbi->bmiColors[0]);
                        mask <<= 1;
                        x++;
                    }
                }
                maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth/8;
            }

            if (icInfo.hbmColor)
            {
                if (GetDIBits(m_cursorDC, icInfo.hbmColor, 0, 0, 0, lpbiColor, 0) == 0)
                {
                    DeleteObject(icInfo.hbmMask);
                    DeleteObject(icInfo.hbmColor);
                    return;
                }

                if (GetDIBits(m_cursorDC, icInfo.hbmColor, 0, lpbiColor->bmiHeader.biHeight, colorBits, lpbiColor, 0) == 0)
                {
                    DeleteObject(icInfo.hbmMask);
                    DeleteObject(icInfo.hbmColor);
                    return;
                }

                quint32* colorBitsPtr = (quint32*) colorBits;
                int colorStride = lpbiColor->bmiHeader.biSizeImage / lpbiColor->bmiHeader.biHeight;


                // draw xor colored cursor
                for (int y = yPos + cursorHeight-1; y >= yPos; --y)
                {
                    for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                    {
                        for (int k = 0; k < 8; ++k)
                        {
                            if (x > 0 && y > 0 && x < m_ddm.Width && y < m_ddm.Height)
                                xorPixel(data, dataStride, x, y, colorBitsPtr);
                            colorBitsPtr++;
                            x++;
                        }
                    }
                    colorBitsPtr += colorStride - lpbiColor->bmiHeader.biWidth/8 * lpbiColor->bmiHeader.biBitCount;
                }
            }
            else 
            {
                // draw xor cursor
                for (int y = yPos + cursorHeight-1; y >= yPos; --y)
                {
                    for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                    {
                        quint8 mask = *maskBitsPtr++;
                        for (int k = 0; k < 8; ++k)
                        {
                            if ((mask & 0x80) == 0 && x > 0 && y > 0 && x < m_ddm.Width && y < m_ddm.Height)
                                xorPixel(data, dataStride, x, y, (quint32*) &lpbi->bmiColors[1]);
                            mask <<= 1;
                            x++;
                        }
                    }
                    maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth/8;
                }
            }
            DeleteObject(icInfo.hbmMask);
            DeleteObject(icInfo.hbmColor);
        }
    }
}

bool CLScreenGrapper::direct3DDataToFrame(void* opaque, AVFrame* pFrame)
{
    IDirect3DSurface9* pSurface = (IDirect3DSurface9*) opaque;

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
        //pFrame->pict_type = AV_PICTURE_TYPE_I;
        pFrame->width = m_ddm.Width;
        pFrame->height = m_ddm.Height;
        pFrame->format = format();
        pFrame->sample_aspect_ratio.num = 1;
        pFrame->sample_aspect_ratio.den = 1;

    }
    pFrame->coded_picture_number = m_frameNum++;
    pFrame->pts = m_timer.elapsed();
    pFrame->best_effort_timestamp = pFrame->pts;

#if 0
    // speed test
    QTime t1;
    t1.start();
    for (int i = 0; i < 1000; ++i)
    {
        bgra_to_yv12_mmx((unsigned char*)lockedRect.pBits, m_ddm.Width*4, pFrame->data[0], pFrame->data[1], pFrame->data[2], 
            pFrame->linesize[0], pFrame->linesize[1], m_ddm.Width, m_ddm.Height, 0);
        //bgra_yuv420((unsigned char*)lockedRect.pBits, pFrame->data[0], pFrame->data[1], pFrame->data[2], m_ddm.Width, m_ddm.Height);
    }
    int time1 = t1.elapsed();

    QTime t2;
    t2.start();
    for (int i = 0; i < 1000; ++i)

    {
        bgra_to_yv12_sse((unsigned char*)lockedRect.pBits, m_ddm.Width*4, pFrame->data[0], pFrame->data[1], pFrame->data[2], 
            pFrame->linesize[0], pFrame->linesize[1], m_ddm.Width, m_ddm.Height);
    }
    int time2 = t2.elapsed();
    qDebug() << "time1=" << time1 << "time2=" << time2 << "t1/t2=" << time1 / (float) time2;
#else    
    bgra_to_yv12_sse((unsigned char*)lockedRect.pBits, m_ddm.Width*4, pFrame->data[0], pFrame->data[1], pFrame->data[2], 
        pFrame->linesize[0], pFrame->linesize[1], m_ddm.Width, m_ddm.Height);
#endif

    pSurface->UnlockRect();
    return true;
}

bool CLScreenGrapper::openGlDataToFrame(void* opaque, AVFrame* pFrame)
{
    bgra_to_yv12_sse((unsigned char*) opaque, m_ddm.Width*4, pFrame->data[0], pFrame->data[1], pFrame->data[2], 
        pFrame->linesize[0], pFrame->linesize[1], m_ddm.Width, m_ddm.Height);
   return true;
}

bool CLScreenGrapper::capturedDataToFrame(void* opaque, AVFrame* pFrame)
{
    bool rez = false;
    if (m_mode == CaptureMode_Application)
        rez = openGlDataToFrame(opaque, pFrame);
    else 
        rez = direct3DDataToFrame(opaque, pFrame);

    return rez;
}

qint64 CLScreenGrapper::currentTime() const
{
    return m_timer.elapsed();
}
#endif // Q_OS_WIN
