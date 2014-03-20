#include "screen_grabber.h"

#ifdef Q_OS_WIN

#include <QtGui/QScreen>
#include <QtCore/QLibrary>
#include "utils/common/log.h"
#include <emmintrin.h>
#include "utils/media/sse_helper.h"
#include "utils/color_space/yuvconvert.h"

QMutex QnScreenGrabber::m_guiWaitMutex;

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

QnScreenGrabber::QnScreenGrabber(bool captureCursor, const QSize& captureResolution, QWidget* widget):
    m_pD3D(0),
    m_pd3dDevice(0),
    m_frameNum(0),
    m_captureCursor(captureCursor),
    m_captureResolution(captureResolution),
    m_scaleContext(0),
    m_tmpFrame(0),
    m_tmpFrameBuffer(0),
    m_widget(widget),
    m_tmpFrameWidth(0),
    m_tmpFrameHeight(0),
    m_colorBitsCapacity(0),
    m_needStop(false)
{
    qRegisterMetaType<CaptureInfoPtr>();

    memset(&m_rect, 0, sizeof(m_rect));

    m_initialized = InitD3D(GetDesktopWindow());
    m_cursorDC = CreateCompatibleDC(0);
    m_timer.start();
    moveToThread(qApp->thread()); // to allow correct "InvokeMethod" call
}

QnScreenGrabber::~QnScreenGrabber()
{
    pleaseStop();

    delete [] m_openGLData;

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

    DeleteDC(m_cursorDC);

    if (m_scaleContext)
    {
        sws_freeContext(m_scaleContext);
        av_free(m_tmpFrame);
        av_free(m_tmpFrameBuffer);
    }
}

HRESULT        QnScreenGrabber::InitD3D(HWND hWnd)
{
    D3DPRESENT_PARAMETERS        d3dpp;

    if((m_pD3D=Direct3DCreate9(D3D_SDK_VERSION))==NULL)
    {
        cl_log.log(QLatin1String("Unable to Create Direct3D "), cl_logERROR);
        return E_FAIL;
    }

    memset(&m_monInfo, 0, sizeof(m_monInfo));
    m_monInfo.cbSize = sizeof(m_monInfo);
    if (!GetMonitorInfo(m_pD3D->GetAdapterMonitor(0), &m_monInfo))
    {
        cl_log.log(QLatin1String("Unable to determine monitor position. Use default"), cl_logWARNING);
    }

    if(FAILED(m_pD3D->GetAdapterDisplayMode(0 ,&m_ddm)))
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


    QDesktopWidget *desktop = qApp->desktop();
    int width = m_rect.right;
    int height = m_rect.bottom;
    for (int i = 0; i < desktop->screenCount(); i++) {
        QRect geometry = desktop->screenGeometry(i);
        width = qMax(width, geometry.width());
        height = qMax(height, geometry.height());
    }

    m_openGLData = new quint8[width * height * 4];

    m_outWidth = m_ddm.Width;
    m_outHeight = m_ddm.Height;

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


    m_outWidth = m_outWidth & ~7; // round by 8

    return S_OK;
}

void QnScreenGrabber::allocateTmpFrame(int width, int height, PixelFormat format)
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

void QnScreenGrabber::captureFrameOpenGL(CaptureInfoPtr data)
{
    QMutexLocker lock (&m_guiWaitMutex);
    if (data->terminated)
        return;

    //CaptureInfo* data = (CaptureInfo*) opaque;
    glReadBuffer(GL_FRONT);
    if (!m_widget) {
        m_waitCond.wakeOne();
        return;
    }
    QRect rect = m_widget->geometry();
    data->width = m_widget->width() & ~31;
    data->height = m_widget->height() & ~1;
    data->pos = QPoint(rect.left(), rect.top());
    QWidget* parent = (QWidget*) m_widget->parent();
    while (parent)
    {
        rect = parent->geometry();
        data->pos += QPoint(rect.left(), rect.top());
        parent = (QWidget*) parent->parent();
    }

    glReadPixels(0, 0, data->width, data->height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data->opaque);

    m_waitCond.wakeOne();
}

CaptureInfoPtr QnScreenGrabber::captureFrame()
{
    CaptureInfoPtr rez = CaptureInfoPtr(new CaptureInfo());

    if (m_needStop)
        return rez;

    
    rez->opaque = m_openGLData;
    QGenericReturnArgument ret;

    //QMetaObject::invokeMethod(this, "captureFrameOpenGL", Qt::BlockingQueuedConnection, ret, Q_ARG(void*, &rez));
    {
        QMutexLocker lock(&m_guiWaitMutex);
        QMetaObject::invokeMethod(this, "captureFrameOpenGL", Qt::QueuedConnection, ret, Q_ARG(CaptureInfoPtr, rez));
        m_waitCond.wait(&m_guiWaitMutex);
        if (m_needStop) {
            rez->terminated = true;
            return rez;
        }
    }

    if (m_captureCursor)
        drawCursor((quint32*) rez->opaque, rez->width, rez->height, rez->pos.x(), rez->pos.y(), true);

    rez->pts = m_timer.elapsed();
    return rez;
}

static inline void andPixel(quint32* data, int stride, int x, int y, quint32* srcPixel)
{
    data[y*stride+x] &= *srcPixel;
}

static inline void xorPixel(quint32* data, int stride, int x, int y, quint32* srcPixel)
{
    data[y*stride+x] ^= *srcPixel;
}

void QnScreenGrabber::drawLogo(quint8* data, int width, int height)
{
    if (m_logo.width() == 0)
        return;
    int left = width - m_logo.width() - LOGO_CORNER_OFFSET;
    int top = LOGO_CORNER_OFFSET;
    
    QImage buffer(data, width, height, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&buffer);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver); // enable alpha blending
    painter.drawPixmap(QPoint(left, top), m_logo);
}

void QnScreenGrabber::drawCursor(quint32* data, int width, int height, int leftOffset, int topOffset, bool flip) const
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
            quint8 lpbiData[sizeof(BITMAPINFO) + 128]; // 2 additional colors
            quint8 lpbiColorData[sizeof(BITMAPINFO) + 128]; // 4 additional colors

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
                for (int y = yPos - cursorHeight; y < yPos; ++y)
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

                if( m_colorBitsCapacity < lpbiColor->bmiHeader.biSizeImage )
                {
                    m_colorBitsCapacity = lpbiColor->bmiHeader.biSizeImage;
                    m_colorBits.reset( new quint8[lpbiColor->bmiHeader.biSizeImage] );
                }

                if (GetDIBits(m_cursorDC, icInfo.hbmColor, 0, lpbiColor->bmiHeader.biHeight, m_colorBits.get(), lpbiColor, 0) == 0)
                {
                    DeleteObject(icInfo.hbmMask);
                    DeleteObject(icInfo.hbmColor);
                    return;
                }

                quint32* colorBitsPtr = (quint32*) m_colorBits.get();
                int colorStride = lpbiColor->bmiHeader.biSizeImage / lpbiColor->bmiHeader.biHeight;

                // draw xor colored cursor
                if (!flip)
                {
                    for (int y = yPos + cursorHeight-1; y >= yPos; --y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; ++x)
                        {
                            if (x > 0 && y > 0 && x < width && y < height)
                                xorPixel(data, width, x, y, colorBitsPtr);
                            colorBitsPtr++;
                        }
                        colorBitsPtr += colorStride - lpbiColor->bmiHeader.biWidth * lpbiColor->bmiHeader.biBitCount / 8;
                    }
                }
                else
                {
                    for (int y = yPos-cursorHeight; y < yPos; ++y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; ++x)
                        {
                            if (x > 0 && y > 0 && x < width && y < height)
                                xorPixel(data, width, x, y, colorBitsPtr);
                            colorBitsPtr++;
                        }
                        colorBitsPtr += colorStride - lpbiColor->bmiHeader.biWidth * lpbiColor->bmiHeader.biBitCount / 8;
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
                    for (int y = yPos-cursorHeight; y < yPos; ++y)
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

bool QnScreenGrabber::dataToFrame(quint8* data, int dataStride, int width, int height, AVFrame* pFrame)
{
    drawLogo((quint8*) data, width, height);
    int roundWidth = width & ~7;
    
    if (roundWidth != m_tmpFrameWidth || height != m_tmpFrameHeight)
        allocateTmpFrame(roundWidth, height, PIX_FMT_YUV420P);

    if (useSSE3())
    {
        bgra_to_yv12_simd_intr(data, dataStride,
            m_tmpFrame->data[0], m_tmpFrame->data[1], m_tmpFrame->data[2],
            m_tmpFrame->linesize[0], m_tmpFrame->linesize[1], roundWidth, height, true);
    }
    else {
        cl_log.log("CPU does not support SSE3!", cl_logERROR);
    }

    sws_scale(m_scaleContext,
        m_tmpFrame->data, m_tmpFrame->linesize,
        0, m_ddm.Height,
        pFrame->data, pFrame->linesize);
    
    
   return true;
}

bool QnScreenGrabber::capturedDataToFrame(CaptureInfoPtr captureInfo, AVFrame* pFrame)
{
    static const int RGBA32_BYTES_PER_PIXEL = 4;

    bool rez = dataToFrame((quint8*) captureInfo->opaque, captureInfo->width*RGBA32_BYTES_PER_PIXEL, captureInfo->width, captureInfo->height, pFrame);
    pFrame->pts = captureInfo->pts;
    return rez;
}

qint64 QnScreenGrabber::currentTime() const
{
    return m_timer.elapsed();
}

int QnScreenGrabber::width() const
{
    return m_outWidth;
}

int QnScreenGrabber::height() const
{
    return m_outHeight;
}

void QnScreenGrabber::setLogo(const QPixmap& logo)
{
    m_logo = QPixmap::fromImage(logo.toImage().mirrored(false, true));
    m_logo = m_logo.scaledToWidth(screenWidth() * 0.085, Qt::SmoothTransformation);
}

int QnScreenGrabber::screenWidth() const
{
    return m_ddm.Width;
}

int QnScreenGrabber::screenHeight() const
{
    return m_ddm.Height;
}

void QnScreenGrabber::pleaseStop()
{
    QMutexLocker lock(&m_guiWaitMutex);
    m_needStop = true;
    m_waitCond.wakeAll();
}

#endif // Q_OS_WIN
