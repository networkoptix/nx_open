#include "buffered_screen_grabber.h"

#ifdef Q_OS_WIN

CLBufferedScreenGrabber::CLBufferedScreenGrabber(int displayNumber, int queueSize, int frameRate):
    m_grabber(displayNumber, queueSize),
    m_queue(queueSize),
    m_frameRate(frameRate),
    m_frameIndex(0)
{
    m_frames.resize(queueSize);
    for (int i = 0; i < m_frames.size(); ++i)
        m_frames[i] = avcodec_alloc_frame();
    m_timer.start();
    start(QThread::HighPriority);
}

CLBufferedScreenGrabber::~CLBufferedScreenGrabber()
{
    m_needStop = true;
    wait();
}

void CLBufferedScreenGrabber::run()
{
    while (!m_needStop)
    {
        if (!m_needStop && m_queue.size() == m_queue.maxSize())
        {
            msleep(1);
            continue;
        }
        if (m_needStop)
            break;
        AVFrame* curFrame = m_frames[m_frameIndex];
        m_frameIndex = m_frameIndex < m_frames.size()-1 ? m_frameIndex+1 : 0;
        m_queue.push(m_grabber.captureFrame());

        qint64 nextTiming = (curFrame->coded_picture_number+1)*1000/m_frameRate;
        int toSleep = nextTiming - m_timer.elapsed();
        //qDebug() << "sleep time=" << toSleep;
        if (toSleep > 0)
            msleep(toSleep);
    }
}

IDirect3DSurface9* CLBufferedScreenGrabber::getNextFrame() 
{ 
    IDirect3DSurface9* rez = 0;
    if (m_queue.pop(rez, 40))
        return rez;
    else
        return 0;
}

AVRational CLBufferedScreenGrabber::getFrameRate()
{
    AVRational rez;
    rez.num = 1;
    rez.den = m_frameRate;
    return rez;
}

#endif // Q_OS_WIN

