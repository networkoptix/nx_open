#include "buffered_frame_displayer.h"
#include <utils/media/frame_info.h>
#include <utils/common/adaptive_sleep.h>
#include "video_stream_display.h"
#include "abstract_renderer.h"

QnBufferedFrameDisplayer::QnBufferedFrameDisplayer(): 
    m_queue(11)
{
    m_currentTime = AV_NOPTS_VALUE;
    m_expectedTime = AV_NOPTS_VALUE;
    m_lastDisplayedTime = AV_NOPTS_VALUE;
    start();
}

void QnBufferedFrameDisplayer::setRenderList(QSet<QnAbstractRenderer*> renderList)
{
    SCOPED_MUTEX_LOCK( lock, &m_renderMtx);
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

qint64 QnBufferedFrameDisplayer::bufferedDuration() {
    SCOPED_MUTEX_LOCK( lock, &m_sync);
    if (m_queue.size() == 0)
        return 0;
    else
        return m_lastQueuedTime - m_queue.front()->pkt_dts;
}

bool QnBufferedFrameDisplayer::addFrame(const QSharedPointer<CLVideoDecoderOutput>& outFrame) 
{
    bool wasWaiting = false;
    bool needWait;
    do {
        m_sync.lock();
        m_lastQueuedTime = outFrame->pkt_dts;
        needWait = !needToStop() && (m_queue.size() == m_queue.maxSize() 
                    || (m_queue.size() > 0 && outFrame->pkt_dts - m_queue.front()->pkt_dts >= MAX_QUEUE_TIME));
        m_sync.unlock();
        if (needWait) {
            wasWaiting = true;
        msleep(1);
        }
    } while (needWait);
    m_queue.push(outFrame);
    return wasWaiting;
}

void QnBufferedFrameDisplayer::setCurrentTime(qint64 time) {
    SCOPED_MUTEX_LOCK( lock, &m_sync);
    m_currentTime = time;
    m_timer.restart();
}

void QnBufferedFrameDisplayer::clear() {
    SCOPED_MUTEX_LOCK( processFrameLock, &m_processFrameMutex );
    m_queue.clear();
    m_currentTime = m_expectedTime = AV_NOPTS_VALUE;
}

qint64 QnBufferedFrameDisplayer::getTimestampOfNextFrameToRender() const 
{
    SCOPED_MUTEX_LOCK( lock, &m_sync);
    return m_lastDisplayedTime;
}

void QnBufferedFrameDisplayer::overrideTimestampOfNextFrameToRender(qint64 value)
{
    SCOPED_MUTEX_LOCK( lock, &m_sync);
    m_lastDisplayedTime = value;
}

void QnBufferedFrameDisplayer::run() 
{
    QSharedPointer<CLVideoDecoderOutput> frame;
    while (!needToStop())
    {
        m_processFrameMutex.lock();
        if (m_queue.size() > 0)
        {
            frame = m_queue.front();

            if ((quint64)m_expectedTime == AV_NOPTS_VALUE) {
                m_alignedTimer.restart();
                m_expectedTime = frame->pkt_dts;
            }

            SCOPED_MUTEX_LOCK( syncLock, &m_sync );
            qint64 expectedTime = m_expectedTime  + m_alignedTimer.elapsed()*1000ll;
            qint64 currentTime = (quint64)m_currentTime != AV_NOPTS_VALUE ? m_currentTime + m_timer.elapsed()*1000ll : expectedTime;
                
            //cl_log.log("djitter:", qAbs(expectedTime - currentTime), cl_logALWAYS);
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

            syncLock.unlock();

            if (sleepTime > 0) {
                msleep(sleepTime/1000);
            }
            else if (sleepTime < -1000000) {
                m_currentTime = m_expectedTime = AV_NOPTS_VALUE;
            }
            /*
            else if (sleepTime < -15000)
            {
                if (m_queue.size() > 1 && sleepTime + (m_queue.at(1)->pkt_dts - frame->pkt_dts) <= 0) 
                {
                    cl_log.log("Late picture skipped at ", frame->pkt_dts/1000000.0, cl_logWARNING);
                    syncLock.relock();
                    m_queue.pop(frame);
                    syncLock.unlock();
                    continue;
                }
                if (sleepTime < -1000000) {
                m_currentTime = m_expectedTime = AV_NOPTS_VALUE;
            }
            }
            */

            syncLock.relock();
            m_lastDisplayedTime = frame->pkt_dts;
            syncLock.unlock();

            {
                SCOPED_MUTEX_LOCK( lock, &m_renderMtx);
                foreach(QnAbstractRenderer* render, m_renderList)
                    render->draw(frame);
            }
            //m_drawer->waitForFrameDisplayed(0);
            syncLock.relock();
            m_queue.pop(frame);
            syncLock.unlock();
            m_processFrameMutex.unlock();
            //cl_log.log("queue size:", m_queue.size(), cl_logALWAYS);
        }
        else {
            m_processFrameMutex.unlock();
            msleep(1);
        }
    }

    SCOPED_MUTEX_LOCK( processFrameLock,  &m_processFrameMutex );
    m_queue.clear();
}
