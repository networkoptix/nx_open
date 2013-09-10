#ifndef __BUFFERED_SCREEN_GRABBER_H
#define __BUFFERED_SCREEN_GRABBER_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include <QtWidgets/QWidget>
#include "screen_grabber.h"
#include "utils/common/long_runnable.h"
#include "utils/common/threadqueue.h"
#include "ui/screen_recording/video_recorder_settings.h"

class QnBufferedScreenGrabber: public QnLongRunnable
{
    Q_OBJECT;
public:
    static const int DEFAULT_QUEUE_SIZE = 8;
    static const int DEFAULT_FRAME_RATE = 0;

    QnBufferedScreenGrabber(int displayNumber = D3DADAPTER_DEFAULT,
                            int queueSize = DEFAULT_QUEUE_SIZE,
                            int frameRate = DEFAULT_FRAME_RATE,
                            Qn::CaptureMode mode = Qn::FullScreenNoAeroMode,
                            bool captureCursor = true,
                            const QSize& captureResolution = QSize(0, 0),
                            QWidget* widget = 0);
    virtual ~QnBufferedScreenGrabber();
    CaptureInfoPtr getNextFrame();
    bool dataExist();

    AVRational getFrameRate();
    PixelFormat format() const { return m_grabber.format(); }
    int width() const          { return m_grabber.width();  }
    int height() const         { return m_grabber.height(); }
    int screenWidth() const    { return m_grabber.screenWidth(); }
    int screenHeight() const         { return m_grabber.screenHeight(); }
    qint64 currentTime() const { return m_grabber.currentTime(); }

    bool capturedDataToFrame(CaptureInfoPtr data, AVFrame* frame) { return m_grabber.capturedDataToFrame(data, frame); }
    void setLogo(const QPixmap& logo) { m_grabber.setLogo(logo); }
    virtual void pleaseStop() override;
protected:
    virtual void run();

private:
    QnScreenGrabber m_grabber;
    int m_frameRate;
    CLThreadQueue<CaptureInfoPtr> m_queue;
    QVector<AVFrame*> m_frames;
    int m_frameIndex;
    //QTime m_timer;
    int m_currentFrameNum;
};

#endif

#endif

