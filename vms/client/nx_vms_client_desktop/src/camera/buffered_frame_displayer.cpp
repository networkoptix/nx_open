// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buffered_frame_displayer.h"

#include <nx/media/ffmpeg/frame_info.h>
#include <utils/common/adaptive_sleep.h>

#include "ui/graphics/items/resource/resource_widget_renderer.h"
#include "video_stream_display.h"

QnBufferedFrameDisplayer::QnBufferedFrameDisplayer():
    base_type(),
    m_queue(11)
{
    m_currentTime = AV_NOPTS_VALUE;
    m_expectedTime = AV_NOPTS_VALUE;
    m_lastDisplayedTime = AV_NOPTS_VALUE;
    start();
}

void QnBufferedFrameDisplayer::setRenderList(std::set<QnResourceWidgetRenderer*> renderList)
{
    NX_MUTEX_LOCKER lock( &m_renderMtx );
    m_renderList = renderList;
}

QnBufferedFrameDisplayer::~QnBufferedFrameDisplayer() {
    stop();
}

void QnBufferedFrameDisplayer::waitForFramesDisplayed()
{
    while (m_queue.size() > 0)
        msleep(1);
}

qint64 QnBufferedFrameDisplayer::bufferedDuration()
{
    NX_MUTEX_LOCKER lock(&m_sync);
    auto randomAccess = m_queue.lock();
    if (randomAccess.size() == 0)
        return 0;
    else
        return m_lastQueuedTime - randomAccess.front()->pkt_dts;
}

bool QnBufferedFrameDisplayer::addFrame(const CLConstVideoDecoderOutputPtr& outFrame)
{
    bool wasWaiting = false;
    bool needWait;
    do
    {
        {
            NX_MUTEX_LOCKER lock(&m_sync);
            m_lastQueuedTime = outFrame->pkt_dts;
            auto randomAccess = m_queue.lock();
            needWait = !needToStop() && (randomAccess.size() == m_queue.maxSize() ||
                (randomAccess.size() > 0 && outFrame->pkt_dts - randomAccess.front()->pkt_dts >= kMaxQueueTime));
        }
        if (needWait)
        {
            wasWaiting = true;
            msleep(1);
        }
    } while (needWait);
    m_queue.push(outFrame);
    return wasWaiting;
}

void QnBufferedFrameDisplayer::setCurrentTime(qint64 time)
{
    NX_MUTEX_LOCKER lock( &m_sync );
    m_currentTime = time;
    m_timer.restart();
}

void QnBufferedFrameDisplayer::clear()
{
    NX_MUTEX_LOCKER lock(&m_sync);
    m_queue.clear();
    m_currentTime = m_expectedTime = AV_NOPTS_VALUE;
    m_sleepCond.wakeOne();
}

qint64 QnBufferedFrameDisplayer::getTimestampOfNextFrameToRender() const
{
    NX_MUTEX_LOCKER lock( &m_sync );
    return m_lastDisplayedTime;
}

void QnBufferedFrameDisplayer::overrideTimestampOfNextFrameToRender(qint64 value)
{
    NX_MUTEX_LOCKER lock( &m_sync );
    m_lastDisplayedTime = value;
}

void QnBufferedFrameDisplayer::pleaseStop()
{
    base_type::pleaseStop();
    m_queue.setTerminated(true);
}

void QnBufferedFrameDisplayer::run()
{
    CLConstVideoDecoderOutputPtr frame;
    m_queue.setTerminated(false);
    while (!needToStop())
    {
        {
            auto randomAccess = m_queue.lock();
            frame = randomAccess.size() > 0 ? randomAccess.front() : CLConstVideoDecoderOutputPtr();
        }
        if (frame)
        {
            if (m_expectedTime == AV_NOPTS_VALUE)
            {
                m_alignedTimer.restart();
                m_expectedTime = frame->pkt_dts;
            }

            NX_MUTEX_LOCKER syncLock( &m_sync );
            qint64 expectedTime = m_expectedTime  + m_alignedTimer.elapsed()*1000ll;
            qint64 currentTime = m_currentTime != AV_NOPTS_VALUE ? m_currentTime + m_timer.elapsed()*1000ll : expectedTime;

            // align to grid
            if (qAbs(expectedTime - currentTime) < 60000) {
                currentTime = expectedTime;
            }
            else {
                m_alignedTimer.restart();
                m_expectedTime = currentTime;
            }
            if (frame->pkt_dts - currentTime >  MAX_VALID_SLEEP_TIME) {
                currentTime = m_currentTime = m_expectedTime = frame->pkt_dts;
                m_alignedTimer.restart();
                m_timer.restart();
            }
            qint64 sleepTime = frame->pkt_dts - currentTime;
            if (sleepTime > 0)
                m_sleepCond.wait(&m_sync, sleepTime / 1000);
            else if (sleepTime < -1000000)
                m_currentTime = m_expectedTime = AV_NOPTS_VALUE;

            if (m_queue.isEmpty())
                continue; //< queue clear could be called from other thread
            m_queue.pop(frame);
            m_lastDisplayedTime = frame->pkt_dts;
            {
                NX_MUTEX_LOCKER lock( &m_renderMtx );
                int maxW = 0, maxH = 0;
                for (const auto& render: m_renderList)
                {
                    QSize sz = render->sizeOnScreen(0);
                    maxW = qMax(sz.width(), maxW);
                    maxH = qMax(sz.height(), maxH);
                }
                foreach(QnResourceWidgetRenderer* render, m_renderList)
                    render->draw(frame, QSize(maxW, maxH));
            }
        }
        else {
            msleep(1);
        }
    }

    m_queue.clear();
}
