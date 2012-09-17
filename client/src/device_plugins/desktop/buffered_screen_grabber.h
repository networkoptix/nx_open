#ifndef __BUFFERED_SCREEN_GRABBER_H
#define __BUFFERED_SCREEN_GRABBER_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include <QtGui/QWidget>
#include "screen_grabber.h"
#include "utils/common/longrunnable.h"
#include "utils/common/threadqueue.h"

class QnBufferedScreenGrabber: public QnLongRunnable
{
    Q_OBJECT;
public:
    static const int DEFAULT_QUEUE_SIZE = 8;
    static const int DEFAULT_FRAME_RATE = 0;

    QnBufferedScreenGrabber(int displayNumber = D3DADAPTER_DEFAULT,
                            int queueSize = DEFAULT_QUEUE_SIZE,
                            int frameRate = DEFAULT_FRAME_RATE,
                            QnScreenGrabber::CaptureMode mode = QnScreenGrabber::CaptureMode_DesktopWithoutAero,
                            bool captureCursor = true,
                            const QSize& captureResolution = QSize(0, 0),
                            QWidget* widget = 0);
    virtual ~QnBufferedScreenGrabber();
    QnScreenGrabber::CaptureInfo getNextFrame();
    bool dataExist();

    AVRational getFrameRate();
    PixelFormat format() const { return m_grabber.format(); }
    int width() const          { return m_grabber.width();  }
    int height() const         { return m_grabber.height(); }
    int screenWidth() const    { return m_grabber.screenWidth(); }
    int screenHeight() const         { return m_grabber.screenHeight(); }
    qint64 currentTime() const { return m_grabber.currentTime(); }

    bool capturedDataToFrame(QnScreenGrabber::CaptureInfo data, AVFrame* frame) { return m_grabber.capturedDataToFrame(data, frame); }
    void stop();
    void setLogo(const QPixmap& logo) { m_grabber.setLogo(logo); }

protected:
    virtual void run();

private:
    QnScreenGrabber m_grabber;
    int m_frameRate;
    CLThreadQueue<QnScreenGrabber::CaptureInfo> m_queue;
    QVector<AVFrame*> m_frames;
    int m_frameIndex;
    //QTime m_timer;
    int m_currentFrameNum;
};

#endif

#endif

