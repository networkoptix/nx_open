#include "common/log.h"
#include "cpull_media_stream_provider.h"

QnClientPullStreamProvider::QnClientPullStreamProvider(QnResourcePtr res):
QnAbstractMediaStreamDataProvider(res)
{

}

QnClientPullStreamProvider::~QnClientPullStreamProvider()
{
    stop();
}

bool QnAbstractMediaStreamDataProvider::beforeGetData()
{
    // does nothing in client pull
    return true;
}

void QnAbstractMediaStreamDataProvider::sleepIfNeeded()
{
    // here some kind of adaptive delay must be inserted based on m_dataRate getDataRate and stats
}
