#ifndef __CL_AVI_BLURAY_DEVICE_H
#define __CL_AVI_BLURAY_DEVICE_H

#include "avi_resource.h"

class CLAviBluRayDevice : public QnAviResource
{
public:
	CLAviBluRayDevice(const QString& file);
	virtual ~CLAviBluRayDevice();

	virtual QnStreamDataProvider* getDeviceStreamConnection();
    static bool isAcceptedUrl(const QString& url);
};

#endif //__CL_AVI_BLURAY_DEVICE_H
