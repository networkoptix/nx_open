#ifndef __CL_AVI_DVD_DEVICE_H
#define __CL_AVI_DVD_DEVICE_H


#include "avi_resource.h"

class CLAviDvdDevice : public QnAviResource
{
public:
	CLAviDvdDevice(const QString& file);
	virtual ~CLAviDvdDevice();

	virtual QnMediaStreamDataProvider* getDeviceStreamConnection();
    static bool isAcceptedUrl(const QString& url);
};


#endif // __CL_AVI_DVD_DEVICE_H
