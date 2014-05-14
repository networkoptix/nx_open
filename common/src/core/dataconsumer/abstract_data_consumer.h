#ifndef abstract_data_consumer_h_2111
#define abstract_data_consumer_h_2111

#include "abstract_data_receptor.h"
#include "utils/common/long_runnable.h"

class QN_EXPORT QnAbstractDataConsumer
:
    public QnLongRunnable,
    public QnAbstractDataReceptor
{
    Q_OBJECT

public:
    QnAbstractDataConsumer(int maxQueueSize);
    virtual ~QnAbstractDataConsumer(){ stop(); }

    /**
      * @return true is there is any space in the queue, false otherwise
      */
    virtual bool canAcceptData() const override;
    virtual void putData(QnAbstractDataPacketPtr data) override;
    virtual void clearUnprocessedData();
    int queueSize() const;
    virtual void setSingleShotMode(bool /*single*/) {}

    virtual void setSpeed(float /*value*/) {}
    //virtual qint64 getDisplayedTime() const { return 0; }
    virtual bool isRealTimeSource() const { return false; }

protected:
    void run();
    virtual bool processData(QnAbstractDataPacketPtr /*data*/)=0;
    virtual void endOfRun();

protected:
    CLDataQueue m_dataQueue;
};

#endif // abstract_data_consumer_h_2111
