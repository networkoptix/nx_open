#ifndef __RTSP_CLIENT_DATA_PROVIDER_H
#define __RTSP_CLIENT_DATA_PROVIDER_H

#include "dataprovider/navigated_dataprovider.h"

class QnRtspClientDataProvider: public QnNavigatedDataProvider
{
public:
    QnRtspClientDataProvider(QnResourcePtr res);
protected:
    virtual QnAbstractDataPacketPtr getNextData();
    virtual void updateStreamParamsBasedOnQuality(); 
    virtual void channeljumpTo(quint64 mksec, int channel);
};

#endif
