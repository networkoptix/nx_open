#ifndef abstract_data_processor_h_2111
#define abstract_data_processor_h_2111

struct CLAbstractData;

#include "../base/longrunnable.h"
#include "data.h"

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
    virtual void setSingleShotMode(bool /*single*/) {}

protected:
	void run();
        virtual bool processData(CLAbstractData* /*data*/)=0;
	virtual void endOfRun();

protected:
	CLDataQueue m_dataQueue;
};

#endif //abstract_data_processor_h_2111
