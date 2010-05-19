#ifndef FakeStreamReader_h_106
#define FakeStreamReader_h_106


#include "../../../streamreader/cpull_stremreader.h"
#include "../../../base/adaptivesleep.h"

class FakeStreamReader : public CLClientPullStreamreader
{
public:
	explicit FakeStreamReader(CLDevice* dev );
	

	~FakeStreamReader();

protected:
	virtual CLAbstractMediaData* getNextData();
	virtual unsigned int getChannelNumber(){return 0;}; // single sensor camera has only one sensor
	
protected:

private:

	static unsigned char* data;
	static unsigned char* descr;
	unsigned char* pdata;
	int* pdescr;

	static int data_len;
	static int descr_data_len;

	CLAdaptiveSleep m_sleep;



};

#endif //FakeStreamReader_h_106