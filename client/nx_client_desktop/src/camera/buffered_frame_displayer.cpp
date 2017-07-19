#include "buffered_frame_displayer.h"
#include <utils/media/frame_info.h>
#include <utils/common/adaptive_sleep.h>
#include "video_stream_display.h"
#include "abstract_renderer.h"

QnBufferedFrameDisplayer::QnBufferedFrameDisplayer():
    base_type(),
    m_queue(11)
{
    m_currentTime = AV_NOPTS_VALUE;
    m_expectedTime = AV_NOPTS_VALUE;
    m_lastDisplayedTime = AV_NOPTS_VALUE;
    start();
}

void QnBufferedFrameDisplayer::setRenderList(QSet<QnAbstractRenderer*> renderList)
{
    QnMutexLocker lock( &m_renderMtx );
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
    QnMutexLocker lock(&m_sync);
    auto randomAccess = m_queue.lock();
    if (randomAccess.size() == 0)
        return 0;
    else
        return m_lastQueuedTime - randomAccess.front()->pkt_dts;
}

bool QnBufferedFrameDisplayer::addFrame(const QSharedPointer<CLVideoDecoderOutput>& outFrame)
{
    bool wasWaiting = false;
    bool needWait;
    do
    {
        {
            QnMutexLocker lock(&m_sync);
            m_lastQueuedTime = outFrame->pkt_dts;
            auto randomAccess = m_queue.lock();
            needWait = !needToStop() && (randomAccess.size() == m_queue.maxSize() ||
                (randomAccess.size() > 0 && outFrame->pkt_dts - randomAccess.front()->pkt_dts >= MAX_QUEUE_TIME));
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
    QnMutexLocker lock( &m_sync );
    m_currentTime = time;
    m_timer.restart();
}

void QnBufferedFrameDisplayer::clear()
{
    QnMutexLocker lock(&m_sync);
    m_queue.clear();
    m_currentTime = m_expectedTime = AV_NOPTS_VALUE;
}

qint64 QnBufferedFrameDisplayer::getTimestampOfNextFrameToRender() const
{
    QnMutexLocker lock( &m_sync );
    return m_lastDisplayedTime;
}

void QnBufferedFrameDisplayer::overrideTimestampOfNextFrameToRender(qint64 value)
{
    QnMutexLocker lock( &m_sync );
    m_lastDisplayedTime = value;
}

void QnBufferedFrameDisplayer::pleaseStop()
{
    base_type::pleaseStop();
    m_queue.setTerminated(true);
}

void QnBufferedFrameDisplayer::run()
{
    QSharedPointer<CLVideoDecoderOutput> frame;
    m_queue.setTerminated(false);
    while (!needToStop())
    {
        {
            auto randomAccess = m_queue.lock();
            frame = randomAccess.size() > 0 ? randomAccess.front() : QSharedPointer<CLVideoDecoderOutput>();
        }
        if (frame)
        {
            if (m_expectedTime == AV_NOPTS_VALUE)
            {
                m_alignedTimer.restart();
                m_expectedTime = frame->pkt_dts;
            }

            QnMutexLocker syncLock( &m_sync );
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
                QnMutexLocker lock( &m_renderMtx );
                foreach(QnAbstractRenderer* render, m_renderList)
                    render->draw(frame);
            }
            //m_drawer->waitForFrameDisplayed(0);
            syncLock.relock();
            if (m_queue.size() > 0)
                m_queue.pop(frame); //< queue clear could be called from other thread
            syncLock.unlock();
        }
        else {
            msleep(1);
        }
    }

    m_queue.clear();
}
