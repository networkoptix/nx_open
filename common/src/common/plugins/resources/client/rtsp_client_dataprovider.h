#ifndef __RTSP_CLIENT_DATA_PROVIDER_H
#define __RTSP_CLIENT_DATA_PROVIDER_H

#include "dataprovider/cpull_media_stream_provider.h"

class QnRtspClientDataProvider: public QnClientPullStreamProvider
{
public:
    QnRtspClientDataProvider(QnResourcePtr res);
protected:
    virtual QnAbstractDataPacketPtr getNextData();
    virtual void updateStreamParamsBasedOnQuality(); 
};

#endif
