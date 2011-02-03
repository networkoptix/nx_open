#include "fake_device.h"
#include "../streamreader/fake_file_streamreader.h"
#include "../device/device_video_layout.h"


CLDeviceList FakeDevice::findDevices()
{
	CLDeviceList result;

	for (int i = 0; i < 0; ++i)
	{
		CLDevice* dev = new FakeDevice();
		dev->setUniqueId(QString("prerecorded ") + QString::number(i));
		result[dev->getUniqueId()] = dev;
	}

	
	/*/
	CLDevice* dev = new FakeDevice4_180();
	dev->setUniqueId(QString("fake180 ") + QString::number(1));
	result[dev->getUniqueId()] = dev;
	/**/


	/*/
	dev = new FakeDevice4_360();
	dev->setUniqueId(QString("fake360 ") + QString::number(1));
	result[dev->getUniqueId()] = dev;
	/**/


	return result;
}


// executing command 
bool FakeDevice::executeCommand(CLDeviceCommand* command)
{
	return true;
}

CLStreamreader* FakeDevice::getDeviceStreamConnection()
{
	return new FakeStreamReader(this, 1);
}
//=========================================================================
class VideoLayout4_180 : public CLDeviceVideoLayout
{
public:
	VideoLayout4_180(){};
	virtual ~VideoLayout4_180(){};
	//returns number of video channels device has
	virtual unsigned int numberOfChannels() const
	{
		return 4;
	}


	virtual unsigned int width() const 
	{
		return 4;
	}


	virtual unsigned int height() const 
	{
		return 1;
	}

	virtual unsigned int h_position(unsigned int channel) const
	{
		return channel;
	}

	virtual unsigned int v_position(unsigned int channel) const
	{
		return 0;
	}

};

class VideoLayout4_360 : public CLDeviceVideoLayout
{
public:
	VideoLayout4_360(){};
	virtual ~VideoLayout4_360(){};
	//returns number of video channels device has
	virtual unsigned int numberOfChannels() const
	{
		return 4;
	}


	virtual unsigned int width() const 
	{
		return 2;
	}


	virtual unsigned int height() const 
	{
		return 2;
	}

	virtual unsigned int h_position(unsigned int channel) const
	{
		return channel%2;
	}

	virtual unsigned int v_position(unsigned int channel) const
	{
		return channel/2;
	}

};




FakeDevice4_180::FakeDevice4_180()
{
	m_videolayout = new VideoLayout4_180();
}


CLStreamreader* FakeDevice4_180::getDeviceStreamConnection()
{
	return new FakeStreamReader(this, 4);
}

FakeDevice4_360::FakeDevice4_360()
{
	m_videolayout = new VideoLayout4_360();
}


CLStreamreader* FakeDevice4_360::getDeviceStreamConnection()
{
	return new FakeStreamReader(this, 4);
}

