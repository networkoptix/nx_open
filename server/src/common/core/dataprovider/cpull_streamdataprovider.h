#ifndef client_pull_stream_reader_h1226
#define client_pull_stream_reader_h1226

#include "streamdataprovider.h"



struct CLAbstractMediaData;

class CLClientPullStreamreader : public CLStreamreader
{
public:
	CLClientPullStreamreader(CLDevice* dev );
	virtual ~CLClientPullStreamreader(){stop();}

protected:
	virtual CLAbstractMediaData* getNextData() = 0;
	

private:
	void run(); // in a loop: takes images from camera and put into queue
};

#endif //client_pull_stream_reader_h1226
