#ifndef cl_fake_recorder_device_h_2141
#define cl_fake_recorder_device_h_2141

#include "recorder_device.h"

class CLFakeRecorderDevice : public CLRecorderDevice
{
public:
	CLFakeRecorderDevice();
	~CLFakeRecorderDevice();

	QString getName() const;
	virtual QString toString() const;

};
#endif //cl_fake_recorder_device_h_2141
