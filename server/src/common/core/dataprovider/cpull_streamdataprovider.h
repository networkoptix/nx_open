#ifndef client_pull_stream_dataprovider_h1226
#define client_pull_stream_dataprovider_h1226


#include "datapacket/mediadatapacket.h"



struct QnAbstractMediaDataPacket;

class CLClientPullStreamreader : public QnMediaStreamDataProvider
{
public:
	CLClientPullStreamreader(QnResource* dev );
	virtual ~CLClientPullStreamreader(){stop();}

protected:
	virtual QnAbstractMediaDataPacketPtr getNextData() = 0;
	

private:
	void run(); // in a loop: takes images from camera and put into queue
};

#endif //client_pull_stream_dataprovider_h1226
