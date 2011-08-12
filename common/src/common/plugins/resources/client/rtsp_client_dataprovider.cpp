#include "rtsp_client_dataprovider.h"

QnRtspClientDataProvider::QnRtspClientDataProvider(QnResourcePtr res): 
    QnNavigatedDataProvider(res)
{

}

QnAbstractDataPacketPtr QnRtspClientDataProvider::getNextData()
{
    return QnAbstractDataPacketPtr(0);
}

void QnRtspClientDataProvider::updateStreamParamsBasedOnQuality()
{

}

void QnRtspClientDataProvider::channeljumpTo(quint64 mksec, int channel)
{

}
