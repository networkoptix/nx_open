#ifndef __BUFFERED_SCREEN_GRABBER_H
#define __BUFFERED_SCREEN_GRABBER_H

#ifdef Q_OS_WIN

#include "base/longrunnable.h"
#include "screen_grabber.h"
#include "base/threadqueue.h"

class CLBufferedScreenGrabber: public CLLongRunnable
{
public:
    static const int DEFAULT_QUEUE_SIZE = 6;
    static const int DEFAULT_FRAME_RATE = 30;

    CLBufferedScreenGrabber(int displayNumber = D3DADAPTER_DEFAULT, 
                            int queueSize = DEFAULT_QUEUE_SIZE, 
                            int frameRate = DEFAULT_FRAME_RATE,
                            CLScreenGrapper::CaptureMode mode = CLScreenGrapper::CaptureMode_DesktopWithoutAero);
                            //CLScreenGrapper::CaptureMode mode = CLScreenGrapper::CaptureMode_Application);
    virtual ~CLBufferedScreenGrabber();
    void* getNextFrame();
    AVRational getFrameRate();
    PixelFormat format() const { return m_grabber.format(); }
    int width() const          { return m_grabber.width();  }
    int height() const         { return m_grabber.height(); }
    qint64 currentTime() const { return m_grabber.currentTime(); }

    bool capturedDataToFrame(void* data, AVFrame* frame) { return m_grabber.capturedDataToFrame(data, frame); }

protected:
    virtual void run();
private:
    CLScreenGrapper m_grabber;
    int m_frameRate;
    CLNonReferredThreadQueue<void*> m_queue;
    QVector<AVFrame*> m_frames;
    int m_frameIndex;
    QTime m_timer;
    int m_currentFrameNum;
};

#endif // Q_OS_WIN

#endif

