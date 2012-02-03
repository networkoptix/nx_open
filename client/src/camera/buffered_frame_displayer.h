#ifndef QN_BUFFERED_FRAME_DISPLAYER_H
#define QN_BUFFERED_FRAME_DISPLAYER_H

#include <QTime>
#include <QMutex>
#include <utils/common/longrunnable.h>
#include <utils/common/threadqueue.h>

class QnAbstractRenderer;
class CLVideoDecoderOutput;

class BufferedFrameDisplayer: public CLLongRunnable 
{
    Q_OBJECT;

public:
    BufferedFrameDisplayer(QnAbstractRenderer *drawer);

    virtual ~BufferedFrameDisplayer();

    void waitForFramesDisplayed();

    qint64 bufferedDuration();

    bool addFrame(CLVideoDecoderOutput* outFrame);

    void setCurrentTime(qint64 time);

    void clear();

    qint64 getLastDisplayedTime();

    void setLastDisplayedTime(qint64 value);

protected:
    virtual void run() override;

private:
    qint64 m_lastQueuedTime;
    qint64 m_expectedTime;
    QTime m_timer;
    QTime m_alignedTimer;
    QnAbstractRenderer *m_drawer;
    qint64 m_currentTime;
    QMutex m_sync;
    qint64 m_lastDisplayedTime;
    CLThreadQueue<CLVideoDecoderOutput *> m_queue;
};

#endif // QN_BUFFERED_FRAME_DISPLAYER_H
