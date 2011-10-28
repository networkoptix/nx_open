#ifndef __CL_AVI_DVD_DEVICE_H
#define __CL_AVI_DVD_DEVICE_H

#include "avi_device.h"

class CLAviDvdDevice : public QnAviResource
{
public:
	CLAviDvdDevice(const QString& file);
	virtual ~CLAviDvdDevice();

    static bool isAcceptedUrl(const QString& url);
    static QString urlToFirstVTS(const QString& url);
protected:
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
};

typedef QSharedPointer<CLAviDvdDevice> CLAviDvdDevicePtr;

#endif // __CL_AVI_DVD_DEVICE_H
