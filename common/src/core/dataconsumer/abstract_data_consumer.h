#ifndef abstract_data_processor_h_2111
#define abstract_data_processor_h_2111

#include "core/datapacket/abstract_data_packet.h"
#include "utils/common/long_runnable.h"

class QN_EXPORT QnAbstractDataConsumer : public QnLongRunnable
{
    Q_OBJECT

public:
    QnAbstractDataConsumer(int maxQueueSize);
    virtual ~QnAbstractDataConsumer(){ stop(); }

    /**
      * @return true is there is any space in the queue, false otherwise
      */
    virtual bool canAcceptData() const;
    virtual void putData(QnAbstractDataPacketPtr data);
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

#endif // abstract_data_processor_h_2111
