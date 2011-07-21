#ifndef __CL_SCREEN_GRABBER_H
#define __CL_SCREEN_GRABBER_H

#include <QObject>

#ifdef Q_OS_WIN

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <QTime>

class CLScreenGrapper: public QObject
{
    Q_OBJECT
public:
    enum CaptureMode {CaptureMode_DesktopWithAero, CaptureMode_DesktopWithoutAero, CaptureMode_Application};

    CLScreenGrapper(int displayNumber, int poolSize, CaptureMode mode, bool captureCursor = true); // primary display by default
    virtual ~CLScreenGrapper();

    // capture screenshot in YUV 4:2:0 format
    // allocate frame data if frame is not initialized
    void* captureFrame();
    bool capturedDataToFrame(void* surface, AVFrame* frame);

    PixelFormat format() const { return PIX_FMT_YUV420P; }
    //PixelFormat format() const { return PIX_FMT_BGRA; }
    int width() const          { return m_ddm.Width; }
    int height() const         { return m_ddm.Height; }
    qint64 currentTime() const;
private:
    HRESULT	InitD3D(HWND hWnd);
    bool openGlDataToFrame(void* opaque, AVFrame* pFrame);
    bool direct3DDataToFrame(void* opaque, AVFrame* pFrame);
    Q_INVOKABLE void captureFrameOpenGL(void* data);
    void drawCursor(quint32* data, int dataStride) const;
private:
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

};

#endif // Q_OS_WIN

#endif