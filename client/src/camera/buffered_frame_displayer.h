#ifndef QN_BUFFERED_FRAME_DISPLAYER_H
#define QN_BUFFERED_FRAME_DISPLAYER_H

#include <QTime>
#include <QMutex>
#include <utils/common/long_runnable.h>
#include <utils/common/threadqueue.h>


class QnAbstractRenderer;
class CLVideoDecoderOutput;

class QnBufferedFrameDisplayer: public QnLongRunnable 
{
    Q_OBJECT;

public:
    QnBufferedFrameDisplayer();

    void setRenderList(QSet<QnAbstractRenderer*> renderList);

    virtual ~QnBufferedFrameDisplayer();

    void waitForFramesDisplayed();

    qint64 bufferedDuration();

    bool addFrame(const QSharedPointer<CLVideoDecoderOutput>& outFrame);

    void setCurrentTime(qint64 time);

    void clear();

    qint64 getTimestampOfNextFrameToRender();

    void overrideTimestampOfNextFrameToRender(qint64 value);

protected:
    virtual void run() override;

private:
    CLThreadQueue<QSharedPointer<CLVideoDecoderOutput>> m_queue;
    QSet<QnAbstractRenderer*> m_renderList;
    qint64 m_lastQueuedTime;
    qint64 m_expectedTime;
    QTime m_timer;
    QTime m_alignedTimer;
    qint64 m_currentTime;
    QMutex m_sync;
    QMutex m_renderMtx;
    qint64 m_lastDisplayedTime;
};

#endif // QN_BUFFERED_FRAME_DISPLAYER_H
