#ifndef __CL_AVI_BLURAY_DEVICE_H
#define __CL_AVI_BLURAY_DEVICE_H

#include "avi_device.h"

class CLAviBluRayDevice : public QnAviResource
{
public:
	CLAviBluRayDevice(const QString& file);
	virtual ~CLAviBluRayDevice();

	virtual QnAbstractStreamDataProvider* createDataProvider(ConnectionRole role);
    static bool isAcceptedUrl(const QString& url);
};

#endif //__CL_AVI_BLURAY_DEVICE_H
