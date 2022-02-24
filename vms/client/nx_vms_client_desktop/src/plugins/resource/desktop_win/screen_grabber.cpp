// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screen_grabber.h"

#include <emmintrin.h>

#include <QtCore/QLibrary>
#include <QtCore/QtMath>

#include <QtGui/QImage>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QPainter>
#include <QtGui/QScreen>

#include <QtWidgets/QApplication>

#include <nx/utils/log/log.h>
#include "utils/media/sse_helper.h"
#include "utils/color_space/yuvconvert.h"

nx::Mutex QnScreenGrabber::m_guiWaitMutex;

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

static const int LOGO_CORNER_OFFSET = 8;

QnScreenGrabber::QnScreenGrabber(int displayNumber,
    int poolSize,
    Qn::CaptureMode mode,
    bool captureCursor,
    const QSize& captureResolution,
    QWidget* widget):
    m_displayNumber(mode == Qn::WindowMode ? 0 : displayNumber),
    m_mode(mode),
    m_poolSize(poolSize + 1),
    m_captureCursor(captureCursor),
    m_cursorDC(CreateCompatibleDC(0)),
    m_captureResolution(captureResolution),
    m_needRescale(captureResolution.width() != 0 || mode == Qn::WindowMode),
    m_widget(widget)
{
    qRegisterMetaType<CaptureInfoPtr>();
    m_initialized = InitD3D(GetDesktopWindow());
    moveToThread(qApp->thread()); // to allow correct "InvokeMethod" call
}

QnScreenGrabber::~QnScreenGrabber()
{
    pleaseStop();

    for (int i = 0; i < m_openGLData.size(); ++i)
        delete[] m_openGLData[i];

    for (int i = 0; i < m_pSurface.size(); ++i)
    {
        if (m_pSurface[i])
        {
            m_pSurface[i]->Release();
            m_pSurface[i] = nullptr;
        }
    }
    if (m_pd3dDevice)
    {
        m_pd3dDevice->Release();
        m_pd3dDevice = nullptr;
    }
    if (m_pD3D)
    {
        m_pD3D->Release();
        m_pD3D = nullptr;
    }
    DeleteDC(m_cursorDC);

    if (m_scaleContext)
    {
        sws_freeContext(m_scaleContext);
        av_free(m_tmpFrame);
        av_free(m_tmpFrameBuffer);
    }
}

HRESULT QnScreenGrabber::InitD3D(HWND hWnd)
{
    D3DPRESENT_PARAMETERS d3dpp;

    if ((m_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
    {
        NX_ERROR(this, "Unable to Create Direct3D");
        return E_FAIL;
    }

    memset(&m_monInfo, 0, sizeof(m_monInfo));
    m_monInfo.cbSize = sizeof(m_monInfo);
    if (!GetMonitorInfo(m_pD3D->GetAdapterMonitor(m_displayNumber), &m_monInfo))
    {
        NX_WARNING(this, "Unable to determine monitor position. Use default");
    }

    if (FAILED(m_pD3D->GetAdapterDisplayMode(m_displayNumber, &m_ddm)))
    {
        NX_ERROR(this, "Unable to Get Adapter Display Mode");
        return E_FAIL;
    }

    ZeroMemory(&d3dpp, sizeof(D3DPRESENT_PARAMETERS));

    d3dpp.Windowed = true; // not fullscreen
    d3dpp.Flags = 0; // D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    d3dpp.BackBufferFormat = m_ddm.Format;
    d3dpp.BackBufferHeight = m_rect.bottom = m_ddm.Height;
    d3dpp.BackBufferWidth = m_rect.right = m_ddm.Width;
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.BackBufferCount = 1;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
    d3dpp.hDeviceWindow = hWnd;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

    if (m_mode != Qn::WindowMode)
    {
        if (FAILED(m_pD3D->CreateDevice(m_displayNumber,
                D3DDEVTYPE_HAL,
                hWnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
                &d3dpp,
                &m_pd3dDevice)))
        {
            qWarning() << "Unable to Create Device";
            return E_FAIL;
        }
        m_pSurface.resize(m_poolSize);
        for (int i = 0; i < m_pSurface.size(); ++i)
        {
            if (FAILED(m_pd3dDevice->CreateOffscreenPlainSurface(m_ddm.Width,
                    m_ddm.Height,
                    D3DFMT_A8R8G8B8,
                    D3DPOOL_SCRATCH,
                    &m_pSurface[i],
                    nullptr)))
            {
                qWarning() << "Unable to Create Surface";
                return E_FAIL;
            }
        }
    }
    else
    {
        const auto screens = QGuiApplication::screens();

        int width = m_rect.right;
        int height = m_rect.bottom;
        for (QScreen* screen: screens)
        {
            const QRect geometry = screen->geometry();
            width = qMax(width, geometry.width());
            height = qMax(height, geometry.height());
        }

        for (int i = 0; i < m_poolSize; ++i)
            m_openGLData << new quint8[width * height * 4];
    }

    m_outWidth = m_ddm.Width;
    m_outHeight = m_ddm.Height;
    if (m_needRescale)
    {
        if (m_captureResolution.width() < 0)
            m_outWidth = m_ddm.Width / qAbs(m_captureResolution.width());
        else if (m_captureResolution.width() == 0)
            m_outWidth = m_ddm.Width;
        else
            m_outWidth = m_captureResolution.width();

        if (m_captureResolution.height() < 0)
            m_outHeight = m_ddm.Height / qAbs(m_captureResolution.height());
        else if (m_captureResolution.height() == 0)
            m_outHeight = m_ddm.Height;
        else
            m_outHeight = m_captureResolution.height();
    }
    m_outWidth = m_outWidth & ~7; // round by 8

    return S_OK;
}

void QnScreenGrabber::allocateTmpFrame(int width, int height, AVPixelFormat format)
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

    m_tmpFrame = av_frame_alloc();
    m_tmpFrame->format = format;
    int numBytes =
        av_image_get_buffer_size(format, m_tmpFrameWidth, m_tmpFrameHeight, /*align*/ 1);
    m_tmpFrameBuffer = (quint8*) av_malloc(numBytes * sizeof(quint8));
    av_image_fill_arrays(m_tmpFrame->data,
        m_tmpFrame->linesize,
        m_tmpFrameBuffer,
        format,
        m_tmpFrameWidth,
        m_tmpFrameHeight,
        /*align*/ 1);

    m_scaleContext = sws_getContext(width,
        height,
        format,
        m_outWidth,
        m_outHeight,
        format,
        SWS_BICUBLIN,
        nullptr,
        nullptr,
        nullptr);
}

void QnScreenGrabber::captureFrameOpenGL(CaptureInfoPtr data)
{
    NX_MUTEX_LOCKER lock(&m_guiWaitMutex);
    if (data->terminated)
        return;

    const auto context = QOpenGLContext::currentContext();
    if (!m_widget || !context)
    {
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

    context->functions()->glReadPixels(
        0, 0, data->width, data->height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data->opaque);

    m_waitCond.wakeOne();
}

CaptureInfoPtr QnScreenGrabber::captureFrame()
{
    CaptureInfoPtr rez = CaptureInfoPtr(new CaptureInfo());

    if (m_needStop)
        return rez;

    if (m_mode == Qn::WindowMode)
    {
        rez->opaque = m_openGLData[m_currentIndex];
        QGenericReturnArgument ret;

        {
            NX_MUTEX_LOCKER lock(&m_guiWaitMutex);
            QMetaObject::invokeMethod(
                this, "captureFrameOpenGL", Qt::QueuedConnection, ret, Q_ARG(CaptureInfoPtr, rez));
            m_waitCond.wait(&m_guiWaitMutex);
            if (m_needStop)
            {
                rez->terminated = true;
                return rez;
            }
        }

        if (m_captureCursor)
            drawCursor(
                (quint32*) rez->opaque, rez->width, rez->height, rez->pos.x(), rez->pos.y(), true);
    }
    else
    { // direct3D capture mode
        if (!m_pd3dDevice)
            return rez;
        HRESULT r = m_pd3dDevice->GetFrontBufferData(0, m_pSurface[m_currentIndex]);
        if (r != D3D_OK)
        {
            NX_WARNING(this, "Unable to capture frame");
            return rez;
        }
        if (m_captureCursor)
        {
            D3DLOCKED_RECT lockedRect;
            if (m_pSurface[m_currentIndex]->LockRect(&lockedRect,
                    &m_rect,
                    D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY)
                == D3D_OK)
            {
                drawCursor((quint32*) lockedRect.pBits,
                    m_ddm.Width,
                    m_ddm.Height,
                    m_monInfo.rcMonitor.left,
                    m_monInfo.rcMonitor.top,
                    false);
                m_pSurface[m_currentIndex]->UnlockRect();
            }
        }
        rez->opaque = m_pSurface[m_currentIndex];
        rez->width = m_ddm.Width;
        rez->height = m_ddm.Height;
    }
    rez->pts = m_timer->elapsed();
    m_currentIndex = m_currentIndex < m_pSurface.size() - 1 ? m_currentIndex + 1 : 0;
    return rez;
}

static inline void andPixel(quint32* data, int stride, int x, int y, quint32* srcPixel)
{
    data[y * stride + x] &= *srcPixel;
}

static inline void xorPixel(quint32* data, int stride, int x, int y, quint32* srcPixel)
{
    data[y * stride + x] ^= *srcPixel;
}

void QnScreenGrabber::drawLogo(quint8* data, int width, int height)
{
    if (m_logo.width() == 0)
        return;
    int left = width - m_logo.width() - LOGO_CORNER_OFFSET;
    int top = m_mode == Qn::WindowMode
        ? LOGO_CORNER_OFFSET
        : height - m_logo.height() - LOGO_CORNER_OFFSET;

    QImage buffer(data, width, height, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&buffer);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver); // enable alpha blending
    painter.drawPixmap(QPoint(left, top), m_logo);
}

void QnScreenGrabber::drawCursor(
    quint32* data, int width, int height, int leftOffset, int topOffset, bool flip) const
{
    static const int MAX_CURSOR_SIZE = 64;
    CURSORINFO pci;
    pci.cbSize = sizeof(CURSORINFO);
    if (GetCursorInfo(&pci))
    {
        if (!(pci.flags & CURSOR_SHOWING))
            return;

        ICONINFO icInfo;
        memset(&icInfo, 0, sizeof(ICONINFO));
        if (GetIconInfo(pci.hCursor, &icInfo))
        {
            int xPos = pci.ptScreenPos.x - ((int) icInfo.xHotspot);
            int yPos = pci.ptScreenPos.y - ((int) icInfo.yHotspot);

            xPos -= leftOffset;
            yPos -= topOffset;

            if (yPos >= 0 && yPos < height && flip)
                yPos = height - yPos;

            quint8 maskBits[MAX_CURSOR_SIZE * MAX_CURSOR_SIZE * 2 / 8 * 16];
            quint8 lpbiData[sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD)];
            quint8 lpbiColorData[sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD)];

            BITMAPINFO* lpbi = (BITMAPINFO*) &lpbiData;
            BITMAPINFO* lpbiColor = (BITMAPINFO*) &lpbiColorData;
            memset(lpbi, 0, sizeof(BITMAPINFO));
            memset(lpbiColor, 0, sizeof(BITMAPINFO));
            lpbi->bmiHeader.biSize = sizeof(BITMAPINFO);
            lpbiColor->bmiHeader.biSize = sizeof(BITMAPINFO);
            if (GetDIBits(m_cursorDC, icInfo.hbmMask, 0, 0, 0, lpbi, DIB_RGB_COLORS) == 0)
            {
                DeleteObject(icInfo.hbmMask);
                DeleteObject(icInfo.hbmColor);
                return;
            }
            if (lpbi->bmiHeader.biClrUsed > 2
                || GetDIBits(m_cursorDC,
                       icInfo.hbmMask,
                       0,
                       lpbi->bmiHeader.biHeight,
                       maskBits,
                       lpbi,
                       DIB_RGB_COLORS)
                    == 0)
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
                for (int y = yPos + cursorHeight - 1; y >= yPos; --y)
                {
                    for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth;)
                    {
                        quint8 mask = *maskBitsPtr++;
                        for (int k = 0; k < 8; ++k)
                        {
                            if ((mask & 0x80) == maskFlag && x > 0 && y > 0 && x < width
                                && y < height)
                                andPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[0]);
                            mask <<= 1;
                            x++;
                        }
                    }
                    maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth / 8;
                }
            }
            else
            {
                for (int y = yPos - cursorHeight; y < yPos; ++y)
                {
                    for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth;)
                    {
                        quint8 mask = *maskBitsPtr++;
                        for (int k = 0; k < 8; ++k)
                        {
                            if ((mask & 0x80) == maskFlag && x > 0 && y > 0 && x < width
                                && y < height)
                                andPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[0]);
                            mask <<= 1;
                            x++;
                        }
                    }
                    maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth / 8;
                }
            }
            if (icInfo.hbmColor)
            {
                if (GetDIBits(m_cursorDC, icInfo.hbmColor, 0, 0, 0, lpbiColor, DIB_RGB_COLORS)
                    == 0)
                {
                    DeleteObject(icInfo.hbmMask);
                    DeleteObject(icInfo.hbmColor);
                    return;
                }

                if (m_colorBitsCapacity < lpbiColor->bmiHeader.biSizeImage)
                {
                    m_colorBitsCapacity = lpbiColor->bmiHeader.biSizeImage;
                    m_colorBits.reset(new quint8[lpbiColor->bmiHeader.biSizeImage]);
                }

                if (GetDIBits(m_cursorDC,
                        icInfo.hbmColor,
                        0,
                        lpbiColor->bmiHeader.biHeight,
                        m_colorBits.get(),
                        lpbiColor,
                        DIB_RGB_COLORS)
                    == 0)
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
                    for (int y = yPos + cursorHeight - 1; y >= yPos; --y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; ++x)
                        {
                            if (x > 0 && y > 0 && x < width && y < height)
                                xorPixel(data, width, x, y, colorBitsPtr);
                            colorBitsPtr++;
                        }
                        colorBitsPtr += colorStride
                            - lpbiColor->bmiHeader.biWidth * lpbiColor->bmiHeader.biBitCount / 8;
                    }
                }
                else
                {
                    for (int y = yPos - cursorHeight; y < yPos; ++y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; ++x)
                        {
                            if (x > 0 && y > 0 && x < width && y < height)
                                xorPixel(data, width, x, y, colorBitsPtr);
                            colorBitsPtr++;
                        }
                        colorBitsPtr += colorStride
                            - lpbiColor->bmiHeader.biWidth * lpbiColor->bmiHeader.biBitCount / 8;
                    }
                }
            }
            else
            {
                // draw xor cursor
                if (!flip)
                {
                    for (int y = yPos + cursorHeight - 1; y >= yPos; --y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth;)
                        {
                            quint8 mask = *maskBitsPtr++;
                            for (int k = 0; k < 8; ++k)
                            {
                                if ((mask & 0x80) == 0 && x > 0 && y > 0 && x < width
                                    && y < height)
                                    xorPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[1]);
                                mask <<= 1;
                                x++;
                            }
                        }
                        maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth / 8;
                    }
                }
                else
                {
                    for (int y = yPos - cursorHeight; y < yPos; ++y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth;)
                        {
                            quint8 mask = *maskBitsPtr++;
                            for (int k = 0; k < 8; ++k)
                            {
                                if ((mask & 0x80) == 0 && x > 0 && y > 0 && x < width
                                    && y < height)
                                    xorPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[1]);
                                mask <<= 1;
                                x++;
                            }
                        }
                        maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth / 8;
                    }
                }
            }
            DeleteObject(icInfo.hbmMask);
            DeleteObject(icInfo.hbmColor);
        }
    }
}

bool QnScreenGrabber::direct3DDataToFrame(void* opaque, AVFrame* pFrame)
{
    IDirect3DSurface9* pSurface = (IDirect3DSurface9*) opaque;

    D3DLOCKED_RECT lockedRect;

    if (FAILED(pSurface->LockRect(
            &lockedRect, 0, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY)))
    {
        qWarning() << "Unable to Lock Front Buffer Surface";
        return false;
    }

    if (pFrame->width == 0)
    {
        int videoBufSize = av_image_get_buffer_size(format(), width(), height(), /*align*/ 1);
        quint8* videoBuf = (quint8*) av_malloc(videoBufSize);
        av_image_fill_arrays(
            pFrame->data, pFrame->linesize, videoBuf, format(), width(), height(), /*align*/ 1);
        pFrame->key_frame = 1;
        // pFrame->pict_type = AV_PICTURE_TYPE_I;
        pFrame->width = width();
        pFrame->height = height();
        pFrame->format = format();
        pFrame->sample_aspect_ratio.num = 1;
        pFrame->sample_aspect_ratio.den = 1;
    }
    pFrame->coded_picture_number = m_frameNum++;
    // pFrame->pts = m_timer.elapsed();
    pFrame->best_effort_timestamp = pFrame->pts;

    bool rez = dataToFrame(
        (unsigned char*) lockedRect.pBits, lockedRect.Pitch, m_ddm.Width, m_ddm.Height, pFrame);
    pSurface->UnlockRect();
    return rez;
}

bool QnScreenGrabber::dataToFrame(
    quint8* data, int dataStride, int width, int height, AVFrame* pFrame)
{
    drawLogo((quint8*) data, width, height);
    int roundWidth = width & ~7;
    if (m_needRescale)
    {
        if (roundWidth != m_tmpFrameWidth || height != m_tmpFrameHeight)
            allocateTmpFrame(roundWidth, height, AV_PIX_FMT_YUV420P);
#if 0 // Perfomance test
        QTime t1, t2;
        t1.start();
        for (int i = 0; i < 1000; ++i)
        {
            bgra_to_yv12_sse_intr(
                data,
                width * 4,
                m_tmpFrame->data[0],
                m_tmpFrame->data[1],
                m_tmpFrame->data[2],
                m_tmpFrame->linesize[0],
                m_tmpFrame->linesize[1],
                roundWidth,
                height,
                m_mode == CaptureMode_Application);
        }
        int time1 = t1.elapsed();
        t2.start();

        for (int i = 0; i < 1000; ++i)
        {
            bgra_to_yv12_sse_intr(
                data,
                width * 4,
                m_tmpFrame->data[0],
                m_tmpFrame->data[1],
                m_tmpFrame->data[2],
                m_tmpFrame->linesize[0],
                m_tmpFrame->linesize[1],
                roundWidth,
                height,
                m_mode == CaptureMode_Application);
        }
        int time2 = t2.elapsed();
        qDebug() << "time1=" << time1 << " time2=" << time2;
#else
        if (useSSE3())
        {
            bgra_to_yv12_simd_intr(data,
                dataStride,
                m_tmpFrame->data[0],
                m_tmpFrame->data[1],
                m_tmpFrame->data[2],
                m_tmpFrame->linesize[0],
                m_tmpFrame->linesize[1],
                roundWidth,
                height,
                m_mode == Qn::WindowMode);
        }
        else
        {
            NX_ERROR(this, "CPU does not support SSE3!");
        }
#endif
        sws_scale(m_scaleContext,
            m_tmpFrame->data,
            m_tmpFrame->linesize,
            0,
            height,
            pFrame->data,
            pFrame->linesize);
    }
    else
    {
        if (useSSE3())
        {
            bgra_to_yv12_simd_intr(data,
                dataStride,
                pFrame->data[0],
                pFrame->data[1],
                pFrame->data[2],
                pFrame->linesize[0],
                pFrame->linesize[1],
                roundWidth,
                height,
                m_mode == Qn::WindowMode);
        }
        else
        {
            NX_ERROR(this, "CPU does not support SSE3!");
        }
    }
    return true;
}

bool QnScreenGrabber::capturedDataToFrame(CaptureInfoPtr captureInfo, AVFrame* pFrame)
{
    static const int RGBA32_BYTES_PER_PIXEL = 4;

    bool rez = false;
    if (m_mode == Qn::WindowMode)
        rez = dataToFrame((quint8*) captureInfo->opaque,
            captureInfo->width * RGBA32_BYTES_PER_PIXEL,
            captureInfo->width,
            captureInfo->height,
            pFrame);
    else
        rez = direct3DDataToFrame(captureInfo->opaque, pFrame);
    pFrame->pts = captureInfo->pts;
    return rez;
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
    if (m_mode == Qn::WindowMode)
        m_logo = QPixmap::fromImage(logo.toImage().mirrored(false, true));
    else
        m_logo = logo;
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
    NX_MUTEX_LOCKER lock(&m_guiWaitMutex);
    m_needStop = true;
    m_waitCond.wakeAll();
}

void QnScreenGrabber::restart()
{
    m_needStop = false;
}
