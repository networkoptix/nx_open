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
    QnBufferedFrameDisplayer(QnAbstractRenderer *drawer);

    virtual ~QnBufferedFrameDisplayer();

    void waitForFramesDisplayed();

    qint64 bufferedDuration();

    bool addFrame(const QSharedPointer<CLVideoDecoderOutput>& outFrame);

    void setCurrentTime(qint64 time);

    void clear();

    qint64 getLastDisplayedTime();

    void setLastDisplayedTime(qint64 value);

protected:
    virtual void run() override;

private:
    CLThreadQueue<QSharedPointer<CLVideoDecoderOutput>> m_queue;
    QnAbstractRenderer *m_drawer;
    qint64 m_lastQueuedTime;
    qint64 m_expectedTime;
    QTime m_timer;
    QTime m_alignedTimer;
    qint64 m_currentTime;
    QMutex m_sync;
    qint64 m_lastDisplayedTime;
};

#endif // QN_BUFFERED_FRAME_DISPLAYER_H
