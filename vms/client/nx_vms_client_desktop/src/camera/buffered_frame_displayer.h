// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_BUFFERED_FRAME_DISPLAYER_H
#define QN_BUFFERED_FRAME_DISPLAYER_H

#include <set>

#include <QtCore/QElapsedTimer>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/long_runnable.h>

#include <utils/common/threadqueue.h>

class QnResourceWidgetRenderer;
class CLVideoDecoderOutput;

using CLConstVideoDecoderOutputPtr = QSharedPointer<const CLVideoDecoderOutput>;

class QnBufferedFrameDisplayer: public QnLongRunnable
{
    Q_OBJECT

    using base_type = QnLongRunnable;
public:
    QnBufferedFrameDisplayer();

    void setRenderList(std::set<QnResourceWidgetRenderer*> renderList);

    virtual ~QnBufferedFrameDisplayer();

    void waitForFramesDisplayed();

    qint64 bufferedDuration();

    bool addFrame(const CLConstVideoDecoderOutputPtr& outFrame);

    void setCurrentTime(qint64 time);

    void clear();

    qint64 getTimestampOfNextFrameToRender() const;

    void overrideTimestampOfNextFrameToRender(qint64 value);

    virtual void pleaseStop() override;
protected:
    virtual void run() override;

private:
    QnSafeQueue<CLConstVideoDecoderOutputPtr> m_queue;
    std::set<QnResourceWidgetRenderer*> m_renderList;
    qint64 m_lastQueuedTime;
    qint64 m_expectedTime;
    QElapsedTimer m_timer;
    QElapsedTimer m_alignedTimer;
    qint64 m_currentTime;
    mutable nx::Mutex m_sync;
    nx::WaitCondition m_sleepCond;
    //!This mutex is used for clearing frame queue only
    nx::Mutex m_renderMtx;
    qint64 m_lastDisplayedTime;
};

#endif // QN_BUFFERED_FRAME_DISPLAYER_H
