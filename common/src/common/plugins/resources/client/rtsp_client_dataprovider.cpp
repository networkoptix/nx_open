#include "rtsp_client_dataprovider.h"

QnRtspClientDataProvider::QnRtspClientDataProvider(QnResourcePtr res): 
    QnClientPullStreamProvider(res)
{

}

QnAbstractDataPacketPtr QnRtspClientDataProvider::getNextData()
{
    return QnAbstractDataPacketPtr(0);
}

void QnRtspClientDataProvider::updateStreamParamsBasedOnQuality()
{

}
