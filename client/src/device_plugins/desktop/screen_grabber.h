#ifndef QN_SCREEN_GRABBER_H
#define QN_SCREEN_GRABBER_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include <QtCore/QObject>

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <QTime>

class QnScreenGrabber: public QObject
{
    Q_OBJECT
public:
    struct CaptureInfo
    {
        CaptureInfo(): pts(0), opaque(0), w(0), h(0) {}
        qint64 pts;
        void* opaque;
        int w;
        int h;
        QPoint pos;
    };

    enum CaptureMode {CaptureMode_DesktopWithAero, CaptureMode_DesktopWithoutAero, CaptureMode_Application};

    // resolution (0,0) - use default(native resolution)
    // negative resolution - use specified scale factor

    QnScreenGrabber(int displayNumber, int poolSize, CaptureMode mode, bool captureCursor,
                    const QSize& captureResolution, QWidget* widget);
    virtual ~QnScreenGrabber();

    // capture screenshot in YUV 4:2:0 format
    // allocate frame data if frame is not initialized
    CaptureInfo captureFrame();
    bool capturedDataToFrame(const CaptureInfo& captureInfo, AVFrame* frame);

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

private:
    HRESULT	InitD3D(HWND hWnd);
    bool dataToFrame(quint8* data, int width, int height, AVFrame* pFrame);
    bool direct3DDataToFrame(void* opaque, AVFrame* pFrame);
    Q_INVOKABLE void captureFrameOpenGL(void* opaque);
    void drawCursor(quint32* data, int dataStride, int height, int leftOffset, int topOffset, bool flip) const;
    void drawLogo(quint8* data, int width, int height);
    void allocateTmpFrame(int width, int height, PixelFormat format);

private:
    QPixmap m_logo;
    int m_displayNumber;

    IDirect3D9*			m_pD3D;
    IDirect3DDevice9*	m_pd3dDevice;
    QVector<IDirect3DSurface9*>	m_pSurface;
    QVector<quint8*>	m_openGLData;
    RECT		m_rect;
    HRESULT m_initialized;
    D3DDISPLAYMODE	m_ddm;
    QTime m_timer;
    unsigned m_frameNum;
    int m_currentIndex;

    static QMutex m_instanceMutex;
    static int m_aeroInstanceCounter;
    CaptureMode m_mode;
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
};

#endif // Q_OS_WIN

#endif
