#include "av_panoramic.h"
#include "../streamreader/panoramic_cpul_tftp_stremreader.h"
#include "../../../base/log.h"
#include "device/device_video_layout.h"


class AVVideoLayout180 : public CLDeviceVideoLayout
{
public:
	AVVideoLayout180(){};
	virtual ~AVVideoLayout180(){};
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

class AVVideoLayout360 : public CLDeviceVideoLayout
{
public:
	AVVideoLayout360(){};
	virtual ~AVVideoLayout360(){};
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





CLArecontPanoramicDevice::CLArecontPanoramicDevice(int model):
CLAreconVisionDevice(model),
m_hastestPattern(false)
{
	switch(model)
	{
	case AV8360:
	case AV8365:
		m_videolayout = new AVVideoLayout360();
		break;

	case AV8180:
	case AV8185:
		m_videolayout = new AVVideoLayout180();

	}
}

bool CLArecontPanoramicDevice::getDescription()
{
	m_description = "";
	return true;
}

CLStreamreader* CLArecontPanoramicDevice::getDeviceStreamConnection()
{
	cl_log.log("Creating streamreader for ", m_ip.toString(), cl_logDEBUG1);
	return new AVPanoramicClientPullSSTFTPStreamreader(this);
}

bool CLArecontPanoramicDevice::getBaseInfo()
{
	return CLAreconVisionDevice::getBaseInfo();
}

QString CLArecontPanoramicDevice::getFullName() 
{
	QString result;
	QTextStream str(&result);


	str << m_model;

	CLValue dn;
	getParam("Day/Night switcher enable", dn);

	if (dn==1)// this is not 3135 and not 3130; need to check if IRswitcher_present
		str<<"DN";


	return result;

}

bool CLArecontPanoramicDevice::hasTestPattern() const
{
	return m_hastestPattern;
}