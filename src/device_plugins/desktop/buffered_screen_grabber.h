#ifndef __BUFFERED_SCREEN_GRABBER_H
#define __BUFFERED_SCREEN_GRABBER_H

#include <QWidget>
#include "base/longrunnable.h"
#include "screen_grabber.h"
#include "base/threadqueue.h"

class CLBufferedScreenGrabber: public CLLongRunnable
{
public:
    static const int DEFAULT_QUEUE_SIZE = 8;
    static const int DEFAULT_FRAME_RATE = 0;

    CLBufferedScreenGrabber(int displayNumber = D3DADAPTER_DEFAULT,
                            int queueSize = DEFAULT_QUEUE_SIZE,
                            int frameRate = DEFAULT_FRAME_RATE,
                            CLScreenGrabber::CaptureMode mode = CLScreenGrabber::CaptureMode_DesktopWithoutAero,
                            bool captureCursor = true,
                            const QSize& captureResolution = QSize(0, 0),
                            QWidget* widget = 0);
    virtual ~CLBufferedScreenGrabber();
    CLScreenGrabber::CaptureInfo getNextFrame();
    bool dataExist();

    AVRational getFrameRate();
    PixelFormat format() const { return m_grabber.format(); }
    int width() const          { return m_grabber.width();  }
    int height() const         { return m_grabber.height(); }
    int screenWidth() const    { return m_grabber.screenWidth(); }
    int screenHeight() const         { return m_grabber.screenHeight(); }
    qint64 currentTime() const { return m_grabber.currentTime(); }

    bool capturedDataToFrame(CLScreenGrabber::CaptureInfo data, AVFrame* frame) { return m_grabber.capturedDataToFrame(data, frame); }
    void stop();
    void setLogo(const QPixmap& logo) { m_grabber.setLogo(logo); }
protected:
    virtual void run();
private:
    CLScreenGrabber m_grabber;
    int m_frameRate;
    CLNonReferredThreadQueue<CLScreenGrabber::CaptureInfo> m_queue;
    QVector<AVFrame*> m_frames;
    int m_frameIndex;
    //QTime m_timer;
    int m_currentFrameNum;
};

#endif

