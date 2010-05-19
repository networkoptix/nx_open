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

protected:

	FakeDevice()
	{

	}


public:
	static CLDeviceList findDevices();



};

#endif // fake_ss_device_h_2110