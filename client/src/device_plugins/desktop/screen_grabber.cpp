#include "screen_grabber.h"

#include <QtCore/QLibrary>
#include "utils/common/log.h"
#include <emmintrin.h>
#include "utils/media/sse_helper.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}



#ifndef DWM_EC_DISABLECOMPOSITION
#  define DWM_EC_DISABLECOMPOSITION 0
#endif
#ifndef DWM_EC_ENABLECOMPOSITION
#  define DWM_EC_ENABLECOMPOSITION 1
#endif

typedef DECLSPEC_IMPORT HRESULT (STDAPICALLTYPE *fn_DwmEnableComposition) (UINT uCompositionAction);
static fn_DwmEnableComposition DwmEnableComposition = 0;
static const int LOGO_CORNER_OFFSET = 8;

static void toggleAero(bool enable)
{
    static bool resolved = false;
    if (!resolved) {
        QLibrary lib(QLatin1String("Dwmapi"));
        if (lib.load())
            DwmEnableComposition = (fn_DwmEnableComposition)lib.resolve("DwmEnableComposition");
        resolved = true;
    }

    if (DwmEnableComposition)
        DwmEnableComposition(enable ? DWM_EC_ENABLECOMPOSITION : DWM_EC_DISABLECOMPOSITION);
}


QMutex CLScreenGrabber::m_instanceMutex;
int CLScreenGrabber::m_aeroInstanceCounter;

CLScreenGrabber::CLScreenGrabber(int displayNumber, int poolSize, CaptureMode mode, bool captureCursor, const QSize& captureResolution, QWidget* widget):
    m_pD3D(0),
    m_pd3dDevice(0),
    m_displayNumber(displayNumber),
    m_frameNum(0),
    m_currentIndex(0),
    m_mode(mode),
    m_poolSize(poolSize+1),
    m_captureCursor(captureCursor),
    m_captureResolution(captureResolution),
    m_scaleContext(0),
    m_tmpFrame(0),
    m_tmpFrameBuffer(0),
    m_widget(widget),
    m_tmpFrameWidth(0),
    m_tmpFrameHeight(0)
{
    memset(&m_rect, 0, sizeof(m_rect));

    if (m_mode == CaptureMode_DesktopWithoutAero)
    {
        QMutexLocker locker(&m_instanceMutex);
        if (++m_aeroInstanceCounter == 1)
            toggleAero(false);
    }
    m_needRescale = captureResolution.width() != 0 || mode == CaptureMode_Application;
    if (mode == CaptureMode_Application)
        m_displayNumber = 0;

    m_initialized = InitD3D(GetDesktopWindow());
    m_cursorDC = CreateCompatibleDC(0);
    m_timer.start();
}

CLScreenGrabber::~CLScreenGrabber()
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
            toggleAero(true);
    }
    DeleteDC(m_cursorDC);

    if (m_scaleContext)
    {
        sws_freeContext(m_scaleContext);
        av_free(m_tmpFrame);
        av_free(m_tmpFrameBuffer);
    }
}

HRESULT	CLScreenGrabber::InitD3D(HWND hWnd)
{
    D3DPRESENT_PARAMETERS	d3dpp;

    if((m_pD3D=Direct3DCreate9(D3D_SDK_VERSION))==NULL)
    {
        cl_log.log(QLatin1String("Unable to Create Direct3D "), cl_logERROR);
        return E_FAIL;
    }

    memset(&m_monInfo, 0, sizeof(m_monInfo));
    m_monInfo.cbSize = sizeof(m_monInfo);
    if (!GetMonitorInfo(m_pD3D->GetAdapterMonitor(m_displayNumber), &m_monInfo))
    {
        cl_log.log(QLatin1String("Unable to determine monitor position. Use default"), cl_logWARNING);
    }

    if(FAILED(m_pD3D->GetAdapterDisplayMode(m_displayNumber,&m_ddm)))
    {
        cl_log.log(QLatin1String("Unable to Get Adapter Display Mode"), cl_logERROR);
        return E_FAIL;
    }

    ZeroMemory(&d3dpp,sizeof(D3DPRESENT_PARAMETERS));

    d3dpp.Windowed=true; // not fullscreen
    d3dpp.Flags = 0; //D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    d3dpp.BackBufferFormat = m_ddm.Format;
    d3dpp.BackBufferHeight = m_rect.bottom = m_ddm.Height;
    d3dpp.BackBufferWidth = m_rect.right = m_ddm.Width;
    d3dpp.MultiSampleType=D3DMULTISAMPLE_NONE;
    d3dpp.BackBufferCount = 1;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect=D3DSWAPEFFECT_COPY;
    d3dpp.hDeviceWindow=hWnd;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp.FullScreen_RefreshRateInHz=D3DPRESENT_RATE_DEFAULT;


    if (m_mode != CaptureMode_Application)
    {
        if(FAILED(m_pD3D->CreateDevice(m_displayNumber, D3DDEVTYPE_HAL,hWnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &d3dpp,&m_pd3dDevice)))
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

    m_outWidth = m_ddm.Width;
    m_outHeight = m_ddm.Height;
    if (m_needRescale)
    {
        if (m_captureResolution.width() < 0)
            m_outWidth =  m_ddm.Width / qAbs(m_captureResolution.width());
        else if (m_captureResolution.width() == 0)
            m_outWidth =  m_ddm.Width;
        else
            m_outWidth = m_captureResolution.width();

        if (m_captureResolution.height() < 0)
            m_outHeight =  m_ddm.Height / qAbs(m_captureResolution.height());
        else if (m_captureResolution.height() == 0)
            m_outHeight =  m_ddm.Height;
        else
            m_outHeight = m_captureResolution.height();

    }
    m_outWidth = m_outWidth & ~7; // round by 8

    return S_OK;
}

void CLScreenGrabber::allocateTmpFrame(int width, int height, PixelFormat format)
{
    m_tmpFrameWidth = width;
    m_tmpFrameHeight = height;

    if (m_tmpFrame)
    {
        sws_freeContext(m_scaleContext);
        av_free(m_tmpFrame);
        av_free(m_tmpFrameBuffer);
        m_tmpFrame = 0;
    }

    m_tmpFrame = avcodec_alloc_frame();
    m_tmpFrame->format = format;
    int numBytes = avpicture_get_size(format, m_tmpFrameWidth, m_tmpFrameHeight);
    m_tmpFrameBuffer = (quint8*)av_malloc(numBytes * sizeof(quint8));
    avpicture_fill((AVPicture *)m_tmpFrame, m_tmpFrameBuffer, format, m_tmpFrameWidth, m_tmpFrameHeight);

    m_scaleContext = sws_getContext(
        width, height, format,
        m_outWidth, m_outHeight, format,
        SWS_BICUBLIN, NULL, NULL, NULL);
}

void CLScreenGrabber::captureFrameOpenGL(void* opaque)
{
    CaptureInfo* data = (CaptureInfo*) opaque;
    glReadBuffer(GL_FRONT);
    if (!m_widget)
        return;
    QRect rect = m_widget->geometry();
    data->w = m_widget->width() & ~31;
    data->h = m_widget->height() & ~1;
    data->pos = QPoint(rect.left(), rect.top());
    QWidget* parent = (QWidget*) m_widget->parent();
    while (parent)
    {
        rect = parent->geometry();
        data->pos += QPoint(rect.left(), rect.top());
        parent = (QWidget*) parent->parent();
    }

    glReadPixels(0, 0, data->w, data->h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data->opaque);
}

CLScreenGrabber::CaptureInfo CLScreenGrabber::captureFrame()
{
    CaptureInfo rez;

    if (m_mode == CaptureMode_Application)
    {
        rez.opaque = m_openGLData[m_currentIndex];
        QGenericReturnArgument ret;
        QMetaObject::invokeMethod(this, "captureFrameOpenGL", Qt::BlockingQueuedConnection, ret, Q_ARG(void*, &rez));
        if (m_captureCursor)
            drawCursor((quint32*) rez.opaque, rez.w, rez.h, rez.pos.x(), rez.pos.y(), true);
    }
    else
    {   // direct3D capture mode
        if (!m_pd3dDevice)
            return rez;
        HRESULT r = m_pd3dDevice->GetFrontBufferData(0, m_pSurface[m_currentIndex]);
        if(r != D3D_OK)
        {
            cl_log.log(QLatin1String("Unable to capture frame"), cl_logWARNING);
            return rez;
        }
        if (m_captureCursor)
        {
            D3DLOCKED_RECT	lockedRect;
            if(m_pSurface[m_currentIndex]->LockRect(&lockedRect, &m_rect, D3DLOCK_NO_DIRTY_UPDATE|D3DLOCK_NOSYSLOCK|D3DLOCK_READONLY) == D3D_OK)
            {
                drawCursor((quint32*) lockedRect.pBits, m_ddm.Width, m_ddm.Height, m_monInfo.rcMonitor.left, m_monInfo.rcMonitor.top, false);
                m_pSurface[m_currentIndex]->UnlockRect();
            }
        }
        rez.opaque = m_pSurface[m_currentIndex];
    }
    rez.pts = m_timer.elapsed();
    m_currentIndex = m_currentIndex < m_pSurface.size()-1 ? m_currentIndex+1 : 0;
    return rez;
}

void bgra_to_yv12_sse_intr(quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, int yStride, int uvStride, int width, int height, bool flip)
{
	return;

    static const __m128i sse_2000         = _mm_setr_epi16( 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020 );
    static const __m128i sse_00a0         = _mm_setr_epi16( 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210 );
    static const __m128i sse_mask_color   = _mm_setr_epi32( 0x00003fc0, 0x00003fc0, 0x00003fc0, 0x00003fc0);
    static const __m128i sse_01           = _mm_setr_epi16(0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001);

    static const int K = 32768;
    static const __m128i y_r_coeff =  _mm_setr_epi16( 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K );
    static const __m128i y_g_coeff =  _mm_setr_epi16( 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K );
    static const __m128i y_b_coeff =  _mm_setr_epi16( 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K );

    static const __m128i uv_r_coeff = _mm_setr_epi16( 0.439*K,  0.439*K,  0.439*K,  0.439*K,    -0.148*K, -0.148*K, -0.148*K, -0.148*K );
    static const __m128i uv_g_coeff = _mm_setr_epi16(-0.368*K, -0.368*K, -0.368*K, -0.368*K,    -0.291*K, -0.291*K, -0.291*K, -0.291*K );
    static const __m128i uv_b_coeff = _mm_setr_epi16(-0.071*K, -0.071*K, -0.071*K, -0.071*K,     0.439*K,  0.439*K,  0.439*K,  0.439*K );

    int xSteps = width / 8;
    if (flip)
    {
        y += yStride*(height-1);
        u += uvStride*(height/2-1);
        v += uvStride*(height/2-1);
        yStride = -yStride;
        uvStride = -uvStride;
    }

    for (int yLine = 0; yLine < height/2; ++yLine)
    {
        const __m128i *srcLine1 = (const __m128i*) rgba;
        const __m128i *srcLine2 = (const __m128i*) (rgba + xStride);
        quint8* dstY = y;
        quint32* dstU = (quint32*) u;
        quint32* dstV = (quint32*) v;

        for (int x = xSteps; x > 0; --x)
        {
            // prepare first line
            __m128i rgbaB0 = _mm_and_si128(_mm_slli_epi32(_mm_loadu_si128(srcLine1),6), sse_mask_color);
            rgbaB0 = _mm_packs_epi32(rgbaB0, _mm_and_si128(_mm_slli_epi32(_mm_loadu_si128(srcLine1+1),6), sse_mask_color));
            __m128i rgbaG0 = _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine1),2), sse_mask_color);
            rgbaG0 = _mm_packs_epi32(rgbaG0, _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine1+1),2), sse_mask_color));
            __m128i rgbaR0 = _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine1),10), sse_mask_color);
            rgbaR0 = _mm_packs_epi32(rgbaR0, _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine1+1),10), sse_mask_color));
            // prepare second line
            __m128i rgbaB1 = _mm_and_si128(_mm_slli_epi32(_mm_loadu_si128(srcLine2),6), sse_mask_color);
            rgbaB1 = _mm_packs_epi32(rgbaB1, _mm_and_si128(_mm_slli_epi32(_mm_loadu_si128(srcLine2+1),6), sse_mask_color));
            __m128i rgbaG1 = _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine2),2), sse_mask_color);
            rgbaG1 = _mm_packs_epi32(rgbaG1, _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine2+1),2), sse_mask_color));
            __m128i rgbaR1 = _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine2),10), sse_mask_color);
            rgbaR1 = _mm_packs_epi32(rgbaR1, _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine2+1),10), sse_mask_color));
            // calc Y vector
            __m128i yData = _mm_add_epi16(_mm_mulhi_epi16(rgbaB0, y_b_coeff), _mm_add_epi16(_mm_mulhi_epi16(rgbaG0, y_g_coeff), _mm_mulhi_epi16(rgbaR0, y_r_coeff)));
            yData = _mm_srli_epi16(_mm_add_epi16(yData, sse_00a0), 5);
            _mm_storel_epi64((__m128i*) dstY, _mm_packus_epi16(yData, yData));
            yData = _mm_add_epi16(_mm_mulhi_epi16(rgbaB1, y_b_coeff), _mm_add_epi16(_mm_mulhi_epi16(rgbaG1, y_g_coeff), _mm_mulhi_epi16(rgbaR1, y_r_coeff)));
            yData = _mm_srli_epi16(_mm_add_epi16(yData, sse_00a0), 5);
            _mm_storel_epi64((__m128i*) (dstY+yStride), _mm_packus_epi16(yData, yData));
            // calc avarage for UV
            __m128i bAvg = _mm_madd_epi16(_mm_avg_epu16(rgbaB0, rgbaB1), sse_01); // use madd instead of "horizontal add" to prevent SSSE3 requirement
            __m128i gAvg = _mm_madd_epi16(_mm_avg_epu16(rgbaG0, rgbaG1), sse_01);
            __m128i rAvg = _mm_madd_epi16(_mm_avg_epu16(rgbaR0, rgbaR1), sse_01);
            // calc UV vector
            __m128i uv = sse_2000;
            uv = _mm_add_epi16(uv, _mm_mulhi_epi16(_mm_packs_epi32(rAvg, rAvg), uv_r_coeff));
            uv = _mm_add_epi16(uv, _mm_mulhi_epi16(_mm_packs_epi32(gAvg, gAvg), uv_g_coeff));
            uv = _mm_add_epi16(uv, _mm_mulhi_epi16(_mm_packs_epi32(bAvg, bAvg), uv_b_coeff));
            uv = _mm_srli_epi16(uv, 6);
            uv = _mm_packus_epi16(uv, uv);
            *dstV++ = _mm_cvtsi128_si32(uv);
            *dstU++ = _mm_cvtsi128_si32(_mm_srli_epi64(uv, 32));

            dstY += 8;
            srcLine1 += 2;
            srcLine2 += 2;
        }
        u += uvStride;
        v += uvStride;
        y += yStride*2;
        rgba += xStride*2;
    }
}

void bgra_yuv420(quint8* rgba, quint8* yptr, quint8* uptr, quint8* vptr, int width, int height, bool /*flip*/)
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

void CLScreenGrabber::drawLogo(quint8* data, int width, int height)
{
    if (m_logo.width() == 0)
        return;
    int left = width - m_logo.width() - LOGO_CORNER_OFFSET;
    int top = m_mode == CaptureMode_Application ? height - m_logo.height() - LOGO_CORNER_OFFSET : LOGO_CORNER_OFFSET;
    
    QImage buffer(data, width, height, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&buffer);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver); // enable alpha blending
    painter.drawPixmap(QPoint(left, top), m_logo);
}

void CLScreenGrabber::drawCursor(quint32* data, int width, int height, int leftOffset, int topOffset, bool flip) const
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

            xPos -= leftOffset;
            yPos -= topOffset;

            if (yPos >= 0 && yPos < height && flip)
                yPos = height - yPos;

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

            if (!flip)
            {
                for (int y = yPos + cursorHeight-1; y >= yPos; --y)
                {
                    for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                    {
                        quint8 mask = *maskBitsPtr++;
                        for (int k = 0; k < 8; ++k)
                        {
                            if ((mask & 0x80) == maskFlag && x > 0 && y > 0 && x < width && y < height)
                                andPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[0]);
                            mask <<= 1;
                            x++;
                        }
                    }
                    maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth/8;
                }
            }
            else
            {
                for (int y = yPos; y < yPos + cursorHeight; ++y)
                {
                    for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                    {
                        quint8 mask = *maskBitsPtr++;
                        for (int k = 0; k < 8; ++k)
                        {
                            if ((mask & 0x80) == maskFlag && x > 0 && y > 0 && x < width && y < height)
                                andPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[0]);
                            mask <<= 1;
                            x++;
                        }
                    }
                    maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth/8;
                }
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
                if (!flip)
                {
                    for (int y = yPos + cursorHeight-1; y >= yPos; --y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                        {
                            for (int k = 0; k < 8; ++k)
                            {
                                if (x > 0 && y > 0 && x < width && y < height)
                                    xorPixel(data, width, x, y, colorBitsPtr);
                                colorBitsPtr++;
                                x++;
                            }
                        }
                        colorBitsPtr += colorStride - lpbiColor->bmiHeader.biWidth/8 * lpbiColor->bmiHeader.biBitCount;
                    }
                }
                else
                {
                    for (int y = yPos; y < yPos + cursorHeight; ++y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                        {
                            for (int k = 0; k < 8; ++k)
                            {
                                if (x > 0 && y > 0 && x < width && y < height)
                                    xorPixel(data, width, x, y, colorBitsPtr);
                                colorBitsPtr++;
                                x++;
                            }
                        }
                        colorBitsPtr += colorStride - lpbiColor->bmiHeader.biWidth/8 * lpbiColor->bmiHeader.biBitCount;
                    }
                }
            }
            else
            {
                // draw xor cursor
                if (!flip)
                {
                    for (int y = yPos + cursorHeight-1; y >= yPos; --y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                        {
                            quint8 mask = *maskBitsPtr++;
                            for (int k = 0; k < 8; ++k)
                            {
                                if ((mask & 0x80) == 0 && x > 0 && y > 0 && x < width && y < height)
                                    xorPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[1]);
                                mask <<= 1;
                                x++;
                            }
                        }
                        maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth/8;
                    }
                }
                else
                {
                    for (int y = yPos; y < yPos + cursorHeight; ++y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                        {
                            quint8 mask = *maskBitsPtr++;
                            for (int k = 0; k < 8; ++k)
                            {
                                if ((mask & 0x80) == 0 && x > 0 && y > 0 && x < width && y < height)
                                    xorPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[1]);
                                mask <<= 1;
                                x++;
                            }
                        }
                        maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth/8;
                    }
                }
            }
            DeleteObject(icInfo.hbmMask);
            DeleteObject(icInfo.hbmColor);
        }
    }
}

bool CLScreenGrabber::direct3DDataToFrame(void* opaque, AVFrame* pFrame)
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
    //pFrame->pts = m_timer.elapsed();
    pFrame->best_effort_timestamp = pFrame->pts;

    bool rez = dataToFrame((unsigned char*)lockedRect.pBits, m_ddm.Width, m_ddm.Height, pFrame);
    pSurface->UnlockRect();
    return rez;
}

bool CLScreenGrabber::dataToFrame(quint8* data, int width, int height, AVFrame* pFrame)
{
    drawLogo((quint8*) data, width, height);
    int roundWidth = width & ~7;
    if (m_needRescale)
    {
        if (roundWidth != m_tmpFrameWidth || height != m_tmpFrameHeight)
            allocateTmpFrame(roundWidth, height, PIX_FMT_YUV420P);
#if 0   
        // perfomance test
        QTime t1, t2;
        t1.start();
        for (int i = 0; i < 1000; ++i)
        {
            bgra_to_yv12_sse_intr(data, width * 4,
                m_tmpFrame->data[0], m_tmpFrame->data[1], m_tmpFrame->data[2],
                m_tmpFrame->linesize[0], m_tmpFrame->linesize[1], roundWidth, height, m_mode == CaptureMode_Application);
        }
        int time1 = t1.elapsed();
        t2.start();
        
        for (int i = 0; i < 1000; ++i)
        {
            bgra_to_yv12_sse_intr(data, width * 4,
                m_tmpFrame->data[0], m_tmpFrame->data[1], m_tmpFrame->data[2],
                m_tmpFrame->linesize[0], m_tmpFrame->linesize[1], roundWidth, height, m_mode == CaptureMode_Application);
        }
        int time2 = t2.elapsed();
        qDebug() << "time1=" << time1 << " time2=" << time2;
#else
        if (useSSE3())
        {
            bgra_to_yv12_sse_intr(data, width * 4,
                m_tmpFrame->data[0], m_tmpFrame->data[1], m_tmpFrame->data[2],
                m_tmpFrame->linesize[0], m_tmpFrame->linesize[1], roundWidth, height, m_mode == CaptureMode_Application);
        }
        else {
            cl_log.log("CPU does not support SSE3!", cl_logERROR);
        }
#endif
        sws_scale(m_scaleContext,
            m_tmpFrame->data, m_tmpFrame->linesize,
            0, m_ddm.Height,
            pFrame->data, pFrame->linesize);
    }
    else
    {
        if (useSSE3())
        {
            bgra_to_yv12_sse_intr(data, width*4, pFrame->data[0], pFrame->data[1], pFrame->data[2],
                             pFrame->linesize[0], pFrame->linesize[1], roundWidth, height, m_mode == CaptureMode_Application);
        }
        else {
            cl_log.log("CPU does not support SSE3!", cl_logERROR);
        }
    }
   return true;
}

bool CLScreenGrabber::capturedDataToFrame(const CaptureInfo& captureInfo, AVFrame* pFrame)
{
    bool rez = false;
    if (m_mode == CaptureMode_Application)
        rez = dataToFrame((quint8*) captureInfo.opaque, captureInfo.w, captureInfo.h, pFrame);
    else
        rez = direct3DDataToFrame(captureInfo.opaque, pFrame);
    pFrame->pts = captureInfo.pts;
    return rez;
}

qint64 CLScreenGrabber::currentTime() const
{
    return m_timer.elapsed();
}

int CLScreenGrabber::width() const
{
    return m_outWidth;
}

int CLScreenGrabber::height() const
{
    return m_outHeight;
}

void CLScreenGrabber::setLogo(const QPixmap& logo)
{
    if (m_mode == CaptureMode_Application)
        m_logo = QPixmap::fromImage(logo.toImage().mirrored(false, true));
    else
        m_logo = logo;
    m_logo = m_logo.scaledToWidth(screenWidth() * 0.2140, Qt::SmoothTransformation);
}

int CLScreenGrabber::screenWidth() const
{
    return m_ddm.Width;
}

int CLScreenGrabber::screenHeight() const
{
    return m_ddm.Height;
}
