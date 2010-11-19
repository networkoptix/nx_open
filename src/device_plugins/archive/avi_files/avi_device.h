#ifndef fake_ss_device_h_2110
#define fake_ss_device_h_2110

#include "../../../device/device.h"


// this class and inhereted must be very light to create 
class FakeDevice : public CLDevice
{
public:
	// executing command 
	virtual bool executeCommand(CLDeviceCommand* command);

	virtual CLStreamreader* getDeviceStreamConnection();

	DeviceType getDeviceType() const
	{
		return VIDEODEVICE;
	}


protected:

	FakeDevice()
	{

	}


public:
	static CLDeviceList findDevices();



};

class FakeDevice4_180 : public FakeDevice
{
public:
	FakeDevice4_180();
	virtual CLStreamreader* getDeviceStreamConnection();
protected:
	
};


class FakeDevice4_360 : public FakeDevice
{
public:
	FakeDevice4_360();
	virtual CLStreamreader* getDeviceStreamConnection();
protected:
	
};


#endif // fake_ss_device_h_2110