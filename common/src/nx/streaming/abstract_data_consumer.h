#ifndef abstract_data_consumer_h_2111
#define abstract_data_consumer_h_2111

#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/streaming/data_packet_queue.h>

class QN_EXPORT QnAbstractDataConsumer
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
    virtual void clearUnprocessedData();
    int queueSize() const;
    int maxQueueSize() const;
    virtual void setSingleShotMode(bool /*single*/) {}

    //virtual qint64 getDisplayedTime() const { return 0; }
    virtual bool isRealTimeSource() const { return false; }
    virtual void pleaseStop() override;
protected:

    friend class QnArchiveStreamReader;

    void run();
    virtual void setSpeed(float /*value*/) {}
    virtual bool processData(const QnAbstractDataPacketPtr& data) = 0;
    virtual void beforeRun();
    virtual void endOfRun();
private:
    void resumeDataQueue();
protected:
    QnDataPacketQueue m_dataQueue;
private:
    QnMutex m_pleaseStopMutex;
};

#endif // abstract_data_consumer_h_2111
