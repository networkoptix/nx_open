#ifndef QN_SCREEN_GRABBER_H
#define QN_SCREEN_GRABBER_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include <QtCore/QObject>

// TODO: #Elric move system includes into implementation
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <QtCore/QTime>
#include "ui/screen_recording/video_recorder_settings.h"

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

Q_DECLARE_METATYPE(CaptureInfoPtr);


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

    PixelFormat format() const { return PIX_FMT_YUV420P; }
    //PixelFormat format() const { return PIX_FMT_BGRA; }
    int width() const;
    int height() const;
    qint64 currentTime() const;
    int refreshRate() const { return m_ddm.RefreshRate;}
    void restartTimer() { m_timer.restart(); }
    void setLogo(const QPixmap& logo);
    int screenWidth() const;
    int screenHeight() const;
    void pleaseStop();
private:
    HRESULT        InitD3D(HWND hWnd);
    bool dataToFrame(quint8* data, int dataStride, int width, int height, AVFrame* pFrame);
    bool direct3DDataToFrame(void* opaque, AVFrame* pFrame);
    Q_INVOKABLE void captureFrameOpenGL(CaptureInfoPtr data);
    void drawCursor(quint32* data, int dataStride, int height, int leftOffset, int topOffset, bool flip) const;
    void drawLogo(quint8* data, int width, int height);
    void allocateTmpFrame(int width, int height, PixelFormat format);

private:
    QPixmap m_logo;
    int m_displayNumber;

    IDirect3D9*                        m_pD3D;
    IDirect3DDevice9*        m_pd3dDevice;
    QVector<IDirect3DSurface9*>        m_pSurface;
    QVector<quint8*>        m_openGLData;
    RECT                m_rect;
    HRESULT m_initialized;
    D3DDISPLAYMODE        m_ddm;
    QTime m_timer;
    unsigned m_frameNum;
    int m_currentIndex;

    static QnMutex m_instanceMutex;
    static int m_aeroInstanceCounter;
    Qn::CaptureMode m_mode;
    int m_poolSize;
    bool m_captureCursor;
    HDC m_cursorDC;
    QSize m_captureResolution;
    bool m_needRescale;
    SwsContext* m_scaleContext;
    int m_outWidth;
    int m_outHeight;
    AVFrame* m_tmpFrame;
    quint8* m_tmpFrameBuffer;
    MONITORINFO m_monInfo;
    QWidget* m_widget;
    int m_tmpFrameWidth;
    int m_tmpFrameHeight;
    mutable std::auto_ptr<quint8> m_colorBits;
    mutable size_t m_colorBitsCapacity;

    static QnMutex m_guiWaitMutex;
    QnWaitCondition m_waitCond;
    bool m_needStop;
};

#endif // Q_OS_WIN

#endif
