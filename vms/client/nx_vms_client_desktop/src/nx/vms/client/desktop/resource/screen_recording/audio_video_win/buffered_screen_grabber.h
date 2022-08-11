// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtWidgets/QWidget>

#include <nx/utils/thread/long_runnable.h>
#include <utils/common/threadqueue.h>

#include "../video_recorder_settings.h"
#include "screen_grabber.h"

namespace nx::vms::client::desktop {

class BufferedScreenGrabber: public QnLongRunnable
{
public:
    static const int DEFAULT_QUEUE_SIZE = 8;
    static const int DEFAULT_FRAME_RATE = 0;

    BufferedScreenGrabber(
        int displayNumber = D3DADAPTER_DEFAULT,
        int queueSize = DEFAULT_QUEUE_SIZE,
        int frameRate = DEFAULT_FRAME_RATE,
        Qn::CaptureMode mode = Qn::FullScreenNoAeroMode,
        bool captureCursor = true,
        const QSize& captureResolution = QSize(0, 0),
        QWidget* widget = nullptr);
    virtual ~BufferedScreenGrabber();
    CaptureInfoPtr getNextFrame();
    bool dataExist();

    AVRational getFrameRate();
    AVPixelFormat format() const { return m_grabber.format(); }
    int width() const          { return m_grabber.width();  }
    int height() const         { return m_grabber.height(); }
    int screenWidth() const    { return m_grabber.screenWidth(); }
    int screenHeight() const         { return m_grabber.screenHeight(); }

    bool capturedDataToFrame(CaptureInfoPtr data, AVFrame* frame) { return m_grabber.capturedDataToFrame(data, frame); }
    void setLogo(const QPixmap& logo) { m_grabber.setLogo(logo); }
    virtual void pleaseStop() override;
    void setTimer(QElapsedTimer* timer);

protected:
    virtual void run();
    qint64 currentTime() const { return m_timer->elapsed(); }

private:
    ScreenGrabber m_grabber;
    int m_frameRate;
    QnSafeQueue<CaptureInfoPtr> m_queue;
    QVector<AVFrame*> m_frames;
    int m_frameIndex;
    int m_currentFrameNum;
    static nx::Mutex m_instanceMutex;
    static int m_aeroInstanceCounter;
    QElapsedTimer* m_timer = nullptr;
};

} // namespace nx::vms::client::desktop
