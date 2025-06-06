// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buffered_screen_grabber.h"

#include <QtCore/QLibrary>

namespace {

#ifndef DWM_EC_DISABLECOMPOSITION
#  define DWM_EC_DISABLECOMPOSITION 0
#endif
#ifndef DWM_EC_ENABLECOMPOSITION
#  define DWM_EC_ENABLECOMPOSITION 1
#endif

    typedef DECLSPEC_IMPORT HRESULT (STDAPICALLTYPE *fn_DwmEnableComposition) (UINT uCompositionAction);
    static fn_DwmEnableComposition DwmEnableComposition = 0;
    static const int LOGO_CORNER_OFFSET = 8;
    static const int MAX_JITTER = 60;

} //namespace

namespace nx::vms::client::desktop {

BufferedScreenGrabber::BufferedScreenGrabber(
    int displayNumber,
    int queueSize,
    int frameRate,
    screen_recording::CaptureMode mode,
    bool captureCursor,
    const QSize& captureResolution,
    QWidget* widget)
    :
    m_grabber(displayNumber, queueSize, mode, captureCursor, captureResolution, widget),
    m_frameRate(frameRate),
    m_queue(queueSize),
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
        m_frames[i] = av_frame_alloc();
}

BufferedScreenGrabber::~BufferedScreenGrabber()
{
    stop();
}

void BufferedScreenGrabber::pleaseStop()
{
    m_grabber.pleaseStop();
    QnLongRunnable::pleaseStop();
}

void BufferedScreenGrabber::run()
{
    m_grabber.restart();
    while (!needToStop())
    {
        if (!needToStop() && m_queue.size() == m_queue.maxSize())
        {
            msleep(1);
            continue;
        }
        if (needToStop())
            break;
        m_frameIndex = m_frameIndex < m_frames.size()-1 ? m_frameIndex+1 : 0;
        CaptureInfoPtr info = m_grabber.captureFrame();
        if (!info || info->opaque == 0)
            continue;
        m_queue.push(info);

        qint64 nextTiming = ++m_currentFrameNum * 1000 / m_frameRate;

        int toSleep = nextTiming - currentTime();
        if (toSleep > 0)
            msleep(toSleep);
        else if (toSleep <= -MAX_JITTER)
        {
            m_currentFrameNum = currentTime() * m_frameRate / 1000.0;
        }
    }
}

bool BufferedScreenGrabber::dataExist()
{
    return m_queue.size() > 0;
}

CaptureInfoPtr BufferedScreenGrabber::getNextFrame()
{
    CaptureInfoPtr rez;
    m_queue.pop(rez, std::chrono::milliseconds(40));
    return rez;
}

AVRational BufferedScreenGrabber::getFrameRate()
{
    AVRational rez;
    rez.num = 1;
    rez.den = m_frameRate;
    return rez;
}

void BufferedScreenGrabber::setTimer(QElapsedTimer* timer)
{
    m_timer = timer;
    m_grabber.setTimer(timer);
}

} // namespace nx::vms::client::desktop
