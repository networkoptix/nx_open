// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <windows.h>
//^ Windows header must be included first (actual for no-pch build only).

#include <commdlg.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <shellapi.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtGui/QPixmap>

#include <nx/utils/thread/wait_condition.h>
#include <nx/vms/client/desktop/resource/screen_recording/types.h>

extern "C"
{
struct SwsContext;

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class QWidget;

namespace nx::vms::client::desktop {

struct CaptureInfo
{
    qint64 pts = 0;
    void* opaque = nullptr;
    int width = 0;
    int height = 0;
    QPoint pos;
    bool terminated = false;
};

using CaptureInfoPtr = QSharedPointer<CaptureInfo>;

class ScreenGrabber: public QObject
{
    Q_OBJECT

public:
    // resolution (0,0) - use default(native resolution)
    // negative resolution - use specified scale factor

    ScreenGrabber(
        int displayNumber,
        int poolSize,
        screen_recording::CaptureMode mode,
        bool captureCursor,
        const QSize& captureResolution,
        QWidget* widget);
    virtual ~ScreenGrabber();

    // capture screenshot in YUV 4:2:0 format
    // allocate frame data if frame is not initialized
    CaptureInfoPtr captureFrame();
    bool capturedDataToFrame(CaptureInfoPtr captureInfo, AVFrame* frame);

    AVPixelFormat format() const { return AV_PIX_FMT_YUV420P; }
    //AVPixelFormat format() const { return AV_PIX_FMT_BGRA; }
    int width() const;
    int height() const;
    int refreshRate() const { return m_ddm.RefreshRate;}
    void restart();
    void setLogo(const QPixmap& logo);
    int screenWidth() const;
    int screenHeight() const;
    void pleaseStop();
    screen_recording::CaptureMode getMode() const { return m_mode; }
    void setTimer(QElapsedTimer* timer) { m_timer = timer;  }
private:
    HRESULT        InitD3D(HWND hWnd);
    bool dataToFrame(quint8* data, int dataStride, int width, int height, AVFrame* pFrame);
    bool direct3DDataToFrame(void* opaque, AVFrame* pFrame);
    Q_INVOKABLE void captureFrameOpenGL(CaptureInfoPtr data);
    void drawCursor(quint32* data, int dataStride, int height, int leftOffset, int topOffset, bool flip) const;
    void drawLogo(quint8* data, int width, int height);
    void allocateTmpFrame(int width, int height, AVPixelFormat format);

private:
    QPixmap m_logo;
    const int m_displayNumber;

    IDirect3D9* m_pD3D = nullptr;
    IDirect3DDevice9* m_pd3dDevice = nullptr;
    QVector<IDirect3DSurface9*> m_pSurface;
    QVector<quint8*> m_openGLData;
    RECT m_rect{0, 0, 0, 0};
    D3DDISPLAYMODE m_ddm{};
    QElapsedTimer* m_timer = nullptr;
    int m_currentIndex = 0;

    const screen_recording::CaptureMode m_mode;
    const int m_poolSize;
    const bool m_captureCursor;
    const HDC m_cursorDC;
    const QSize m_captureResolution;
    const bool m_needRescale;
    SwsContext* m_scaleContext = nullptr;
    int m_outWidth;
    int m_outHeight;
    AVFrame* m_tmpFrame = nullptr;
    quint8* m_tmpFrameBuffer = nullptr;
    MONITORINFO m_monInfo{};
    const QWidget* m_widget;
    int m_tmpFrameWidth = 0;
    int m_tmpFrameHeight = 0;
    mutable std::unique_ptr<quint8> m_colorBits;
    mutable size_t m_colorBitsCapacity = 0;

    static nx::Mutex m_guiWaitMutex;
    nx::WaitCondition m_waitCond;
    bool m_needStop = false;

    HRESULT m_initialized = 0;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::CaptureInfoPtr)
