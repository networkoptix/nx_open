#ifndef recorder_device_h
#define recorder_device_h

#include "device\network_device.h"

class CLRecorderDevice : public CLNetworkDevice
{
public:
	CLRecorderDevice();
	~CLRecorderDevice();

	DeviceType getDeviceType() const;

	CLStreamreader* getDeviceStreamConnection();

	bool unknownDevice() const;
	CLNetworkDevice* updateDevice();

protected:
};

#endif //recorder_device_h

