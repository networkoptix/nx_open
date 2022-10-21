// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef abstract_data_consumer_h_2111
#define abstract_data_consumer_h_2111

#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/streaming/data_packet_queue.h>

class NX_VMS_COMMON_API QnAbstractDataConsumer
:
    public QnLongRunnable,
    public QnAbstractMediaDataReceptor
{
    Q_OBJECT

public:
    QnAbstractDataConsumer(int maxQueueSize);
    virtual ~QnAbstractDataConsumer(){ stop(); }

    /**
      * @return true is there is any space in the queue, false otherwise
      */
    virtual bool canAcceptData() const override;
    virtual void putData( const QnAbstractDataPacketPtr& data ) override;
    virtual void clearUnprocessedData() override;
    int queueSize() const;
    int maxQueueSize() const;
    virtual void setSingleShotMode(bool /*single*/) {}

    //virtual qint64 getDisplayedTime() const { return 0; }
    virtual bool isRealTimeSource() const { return false; }
    virtual void pleaseStop() override;
    virtual bool isRunning() const { return QnLongRunnable::isRunning(); }
protected:
    static const int kNoDataDelayIntervalMs = 10;

    friend class QnArchiveStreamReader;

    virtual void run() override;
    virtual void runCycle();
    virtual void setSpeed(float /*value*/) {}
    virtual bool processData(const QnAbstractDataPacketPtr& data) = 0;
    virtual void beforeRun();
    virtual void endOfRun();
private:
    void resumeDataQueue();
protected:
    QnDataPacketQueue m_dataQueue;
private:
    nx::Mutex m_pleaseStopMutex;
};

#endif // abstract_data_consumer_h_2111
