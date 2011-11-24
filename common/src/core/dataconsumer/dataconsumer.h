#ifndef abstract_data_processor_h_2111
#define abstract_data_processor_h_2111

struct QnAbstractDataPacket;



#include "utils/common/longrunnable.h"
#include "../datapacket/datapacket.h"

class QN_EXPORT QnAbstractDataConsumer : public CLLongRunnable
{
    Q_OBJECT;
public:
	QnAbstractDataConsumer(int maxQueueSize);
	virtual ~QnAbstractDataConsumer(){}

    /**
      * @return true is there is any space in the queue, false otherwise
      */
	virtual bool canAcceptData() const; 
	virtual void putData(QnAbstractDataPacketPtr data);
	virtual void clearUnprocessedData();
	int queueSize() const;
    virtual void setSingleShotMode(bool /*single*/) {}

    virtual void setSpeed(float /*value*/) {}
    virtual qint64 currentTime() const { return 0; }


public slots:


protected:
	void run();
    virtual bool processData(QnAbstractDataPacketPtr /*data*/)=0;
	virtual void endOfRun();

protected:
	CLDataQueue m_dataQueue;
};

#endif //abstract_data_processor_h_2111
