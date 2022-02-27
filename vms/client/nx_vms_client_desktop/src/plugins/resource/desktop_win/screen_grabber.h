// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <QtGui/QPixmap>

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <d3d9.h>
#include <d3dx9.h>

#include "ui/screen_recording/video_recorder_settings.h"

#include <nx/utils/thread/wait_condition.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

struct CaptureInfo
{
    CaptureInfo(): pts(0), opaque(0), width(0), height(0), terminated(false) {}
    qint64 pts;
    void* opaque;
    int width;
    int height;
    QPoint pos;
    bool terminated;
};

typedef QSharedPointer<CaptureInfo> CaptureInfoPtr;

Q_DECLARE_METATYPE(CaptureInfoPtr)

class QnScreenGrabber: public QObject
{
    Q_OBJECT
public:


    // resolution (0,0) - use default(native resolution)
    // negative resolution - use specified scale factor

    QnScreenGrabber(int displayNumber, int poolSize, Qn::CaptureMode mode, bool captureCursor,
                    const QSize& captureResolution, QWidget* widget);
    virtual ~QnScreenGrabber();

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
    Qn::CaptureMode getMode() const { return m_mode; }
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
    unsigned m_frameNum = 0;
    int m_currentIndex = 0;

    const Qn::CaptureMode m_mode;
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
