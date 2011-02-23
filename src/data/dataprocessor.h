#ifndef abstract_data_processor_h_2111
#define abstract_data_processor_h_2111

struct CLAbstractData;

#include "../base/longrunnable.h"
#include "data.h"

class CLAbstractDataProcessor : public CLLongRunnable
{
public:
	CLAbstractDataProcessor(int max_que_size);
	virtual ~CLAbstractDataProcessor(){}


	virtual bool canAcceptData() const; // is any space in the queue

	virtual void putData(CLAbstractData* data);
	
	virtual void clearUnProcessedData();
	
	int queSize() const;

protected:
	void run();
	virtual void processData(CLAbstractData* data)=0;
	virtual void endOfRun();

protected:
	CLDataQueue m_dataQueue;
};

#endif //abstract_data_processor_h_2111
