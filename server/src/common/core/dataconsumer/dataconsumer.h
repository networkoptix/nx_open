#ifndef abstract_data_processor_h_2111
#define abstract_data_processor_h_2111

#include "common/longrunnable.h"
#include "datapacket/datapacket.h"

struct CLAbstractData;


class CLAbstractDataProcessor : public CLLongRunnable
{
public:
	CLAbstractDataProcessor(int maxQueueSize);
	virtual ~CLAbstractDataProcessor(){}

    /**
      * @return true is there is any space in the queue, false otherwise
      */
	virtual bool canAcceptData() const; 
	virtual void putData(CLAbstractData* data);
	virtual void clearUnprocessedData();
	int queueSize() const;

protected:
	void run();
	virtual void processData(CLAbstractData* data)=0;
	virtual void endOfRun();

protected:
	CLDataQueue m_dataQueue;
};

#endif //abstract_data_processor_h_2111
