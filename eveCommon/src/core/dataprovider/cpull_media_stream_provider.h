#ifndef client_pull_stream_reader_h1226
#define client_pull_stream_reader_h1226


#include "core/datapacket/mediadatapacket.h"
#include "media_streamdataprovider.h"

struct QnAbstractMediaData;

class QnClientPullMediaStreamProvider : public QnAbstractMediaStreamDataProvider
{
public:
	QnClientPullMediaStreamProvider(QnResourcePtr dev );
	virtual ~QnClientPullMediaStreamProvider(){stop();}

protected:
	virtual QnAbstractMediaDataPtr getNextData() = 0;
	

private:
	void run(); // in a loop: takes images from camera and put into queue
};

#endif //client_pull_stream_reader_h1226
