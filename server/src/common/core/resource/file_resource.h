#ifndef file_device_h_227
#define file_device_h_227

#include "resource.h"

class QnStreamDataProvider;

class CLFileDevice : public QnResource 
{

public:
	CLFileDevice(QString filename);

	DeviceType getDeviceType() const
	{
		return VIDEODEVICE;
	}

	virtual QString toString() const;
	QString getFileName() const;

	virtual QnStreamDataProvider* getDeviceStreamConnection();
protected:

};

#endif // file_device_h_227
