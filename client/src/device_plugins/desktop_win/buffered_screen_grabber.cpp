#include "buffered_screen_grabber.h"

#ifdef Q_OS_WIN

static const int MAX_JITTER = 60;

QnBufferedScreenGrabber::QnBufferedScreenGrabber(int displayNumber,
                                                 int queueSize,
                                                 int frameRate,
                                                 Qn::CaptureMode mode,
                                                 bool captureCursor,
                                                 const QSize& captureResolution,
                                                 QWidget* widget):
    m_grabber(displayNumber, queueSize, mode, captureCursor, captureResolution, widget),
    m_queue(queueSize),
    m_frameRate(frameRate),
    m_frameIndex(0),
    m_currentFrameNum(0)
{
    if (m_frameRate == 0)
    {
        m_frameRate = m_grabber.refreshRate();
        if (m_frameRate % 25 == 0)
            m_frameRate = 25;
        else
            m_frameRate = 30;
    }
    m_frames.resize(queueSize);
    for (int i = 0; i < m_frames.size(); ++i)
        m_frames[i] = avcodec_alloc_frame();
}

QnBufferedScreenGrabber::~QnBufferedScreenGrabber()
{
    stop();
}

void QnBufferedScreenGrabber::pleaseStop()
{
    m_grabber.pleaseStop();
    QnLongRunnable::pleaseStop();
}

void QnBufferedScreenGrabber::run()
{
    m_grabber.restartTimer();
    //m_timer.start();
    while (!needToStop())
    {
        if (!needToStop() && m_queue.size() == m_queue.maxSize())
        {
            msleep(1);
            continue;
        }
        if (needToStop())
            break;
        AVFrame* curFrame = m_frames[m_frameIndex];
        m_frameIndex = m_frameIndex < m_frames.size()-1 ? m_frameIndex+1 : 0;
        CaptureInfoPtr info = m_grabber.captureFrame();
        if (!info || info->opaque == 0)
            continue;
        m_queue.push(info);

        qint64 nextTiming = ++m_currentFrameNum * 1000 / m_frameRate;

        int toSleep = nextTiming - currentTime();
        //cl_log.log("sleep time=", toSleep, cl_logALWAYS);
        if (toSleep > 0)
            msleep(toSleep);
        else if (toSleep <= -MAX_JITTER)
        {
            m_currentFrameNum = currentTime() * m_frameRate / 1000.0;
        }
    }
}

bool QnBufferedScreenGrabber::dataExist()
{
    return m_queue.size() > 0;
}

CaptureInfoPtr QnBufferedScreenGrabber::getNextFrame()
{
    CaptureInfoPtr rez;
    m_queue.pop(rez, 40);
    return rez;
}

AVRational QnBufferedScreenGrabber::getFrameRate()
{
    AVRational rez;
    rez.num = 1;
    rez.den = m_frameRate;
    return rez;
}

#endif // Q_OS_WIN
