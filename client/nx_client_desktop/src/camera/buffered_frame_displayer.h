#ifndef QN_BUFFERED_FRAME_DISPLAYER_H
#define QN_BUFFERED_FRAME_DISPLAYER_H

#include <QtCore/QTime>
#include <nx/utils/thread/mutex.h>

#include <nx/utils/thread/long_runnable.h>
#include <utils/common/threadqueue.h>


class QnAbstractRenderer;
class CLVideoDecoderOutput;

class QnBufferedFrameDisplayer: public QnLongRunnable
{
    Q_OBJECT

    using base_type = QnLongRunnable;
public:
    QnBufferedFrameDisplayer();

    void setRenderList(QSet<QnAbstractRenderer*> renderList);

    virtual ~QnBufferedFrameDisplayer();

    void waitForFramesDisplayed();

    qint64 bufferedDuration();

    bool addFrame(const QSharedPointer<CLVideoDecoderOutput>& outFrame);

    void setCurrentTime(qint64 time);

    void clear();

    qint64 getTimestampOfNextFrameToRender() const;

    void overrideTimestampOfNextFrameToRender(qint64 value);

    virtual void pleaseStop() override;
protected:
    virtual void run() override;

private:
    QnSafeQueue<QSharedPointer<CLVideoDecoderOutput>> m_queue;
    QSet<QnAbstractRenderer*> m_renderList;
    qint64 m_lastQueuedTime;
    qint64 m_expectedTime;
    QTime m_timer;
    QTime m_alignedTimer;
    qint64 m_currentTime;
    mutable QnMutex m_sync;
    QnWaitCondition m_sleepCond;
    //!This mutex is used for clearing frame queue only
    QnMutex m_renderMtx;
    qint64 m_lastDisplayedTime;
};

#endif // QN_BUFFERED_FRAME_DISPLAYER_H
