#ifndef client_pull_stream_dataprovider_h1226
#define client_pull_stream_dataprovider_h1226

#include "media_streamdataprovider.h"

class QnClientPullStreamProvider : public QnAbstractMediaStreamDataProvider
{
public:
	QnClientPullStreamProvider(QnResourcePtr res );
	virtual ~QnClientPullStreamProvider();

protected:
    virtual bool beforeGetData();
    virtual void sleepIfNeeded();

};

#endif //client_pull_stream_dataprovider_h1226
