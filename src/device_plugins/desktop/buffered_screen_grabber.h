#ifndef __BUFFERED_SCREEN_GRABBER_H
#define __BUFFERED_SCREEN_GRABBER_H

#include "base/longrunnable.h"
#include "screen_grabber.h"
#include "base/threadqueue.h"

class IDirect3DSurface9;

class CLBufferedScreenGrabber: public CLLongRunnable
{
public:
    static const int DEFAULT_QUEUE_SIZE = 6;
    static const int DEFAULT_FRAME_RATE = 30;

    CLBufferedScreenGrabber(int displayNumber = D3DADAPTER_DEFAULT, int queueSize = DEFAULT_QUEUE_SIZE, int frameRate = DEFAULT_FRAME_RATE);
    virtual ~CLBufferedScreenGrabber();
    IDirect3DSurface9* getNextFrame();
    AVRational getFrameRate();
    PixelFormat format() const { return m_grabber.format(); }
    int width() const          { return m_grabber.width();  }
    int height() const         { return m_grabber.height(); }

    bool SurfaceToFrame(IDirect3DSurface9* surface, AVFrame* frame) { return m_grabber.SurfaceToFrame(surface, frame); }

protected:
    virtual void run();
private:
    CLScreenGrapper m_grabber;
    int m_frameRate;
    CLNonReferredThreadQueue<IDirect3DSurface9*> m_queue;
    QVector<AVFrame*> m_frames;
    int m_frameIndex;
    QTime m_timer;
};

#endif

