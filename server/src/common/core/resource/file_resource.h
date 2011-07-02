#ifndef file_device_h_227
#define file_device_h_227

#include "resource.h"

class CLStreamreader;

class CLFileDevice : public CLDevice 
{

public:
	CLFileDevice(QString filename);

	DeviceType getDeviceType() const
	{
		return VIDEODEVICE;
	}

	virtual QString toString() const;
	QString getFileName() const;

	virtual CLStreamreader* getDeviceStreamConnection();
protected:

};

#endif // file_device_h_227
