#ifndef server_push_stream_reader_h2055
#define server_push_stream_reader_h2055


#include "media_streamdataprovider.h"
#include "../datapacket/mediadatapacket.h"


struct QnAbstractMediaData;

class CLServerPushStreamreader : public QnAbstractMediaStreamDataProvider
{
public:
	CLServerPushStreamreader(QnResourcePtr dev );
	virtual ~CLServerPushStreamreader(){stop();}

protected:
	
    virtual void openStream() = 0;
    virtual void closeStream() = 0;
    virtual bool isStreamOpened() const = 0;
	

private:
	void run(); // in a loop: takes data from device and puts into queue
};

#endif //server_push_stream_reader_h2055
