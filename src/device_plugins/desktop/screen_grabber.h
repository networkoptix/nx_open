#ifndef __CL_SCREEN_GRABBER_H
#define __CL_SCREEN_GRABBER_H

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <QTime>

class CLScreenGrapper
{
public:
    CLScreenGrapper(int displayNumber, int poolSize); // primary display by default
    virtual ~CLScreenGrapper();

    // capture screenshot in YUV 4:2:0 format
    // allocate frame data if frame is not initialized
    IDirect3DSurface9* captureFrame();
    bool SurfaceToFrame(IDirect3DSurface9* surface, AVFrame* frame);

    PixelFormat format() const { return PIX_FMT_YUV420P; }
    //PixelFormat format() const { return PIX_FMT_BGRA; }
    int width() const          { return m_ddm.Width; }
    int height() const         { return m_ddm.Height; }
protected:
    HRESULT	InitD3D(HWND hWnd);
private:
    int m_displayNumber;

    IDirect3D9*			m_pD3D;
    IDirect3DDevice9*	m_pd3dDevice;
    QVector<IDirect3DSurface9*>	m_pSurface;
    RECT		m_rect;
    HRESULT m_initialized;
    D3DDISPLAYMODE	m_ddm;
    QTime m_timer;
    unsigned m_frameNum;
    int m_currentIndex;

    static QMutex m_instanceMutex;
    static int m_instanceCounter;
};

#endif