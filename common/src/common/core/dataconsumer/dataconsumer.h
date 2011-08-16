#ifndef abstract_data_processor_h_2111
#define abstract_data_processor_h_2111

#include "common/longrunnable.h"
#include "datapacket/datapacket.h"

class QnAbstractDataConsumer : public QnLongRunnable
{
public:
    QnAbstractDataConsumer(int maxQueueSize);
    virtual ~QnAbstractDataConsumer(){}

    /**
      * @return true if there is any space in the queue, false otherwise
      */
    virtual bool canAcceptData() const;
    virtual void putData(QnAbstractDataPacketPtr data);
    virtual void clearUnprocessedData();
    int queueSize() const;

protected:
    void run();
    virtual void processData(QnAbstractDataPacketPtr data) = 0;
    virtual void endOfRun();

protected:
    QnDataPacketQueue m_dataQueue;
};

#endif //abstract_data_processor_h_2111
