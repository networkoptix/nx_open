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
		switch(channel)
		{
		case 0:
			return 0;

		case 1:
			return 2;

		case 2:
		    return 3;

		case 3:
		    return 1;
		default:
		    return 0;
		}
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

bool CLArecontPanoramicDevice::setParam(const QString& name, const CLValue& val )
{
	if (!CLDevice::setParam(name, val))
		return false;

	if (setParam_special(name, val)) // try special first 
		return true;

	CLParamType& value = getDevicePramList().get(name).value;

	//if (value.synchronized && value.value==val) // the same value
	//	return true;




	if (!value.setValue(val, false))
	{
		cl_log.log("cannot set such value!", cl_logWARNING);
		return false;
	}

	if (value.http=="") // check if we have http command for this param
	{
		value.setValue(val);
		return true;
	}


	for (int i = 1; i <=4 ; ++i)
	{
		CLSimpleHTTPClient connection(m_ip, 80, getHttpTimeout(), getAuth());
		QString request;

		QTextStream str(&request);
		str << "set" << i << "?" << value.http;
		if (value.type!=CLParamType::None) str << "=" << (QString)val;

		connection.setRequestLine(request);

		if (connection.openStream()!=CL_HTTP_SUCCESS)
			if (connection.openStream()!=CL_HTTP_SUCCESS) // try twice.
				return false;
	}



	value.setValue(val);
	value.synchronized = true;

	return true;

}

bool CLArecontPanoramicDevice::setParam_special(const QString& name, const CLValue& val)
{
	if (name=="resolution")
	{
		if (val==(QString)("half"))
			return setResolution(false);
		else if (val==(QString)("full"))
			return setResolution(true);

		return false;
	}
	else if (name=="Quality")
	{
		int q = val;
		if (q<1 || q>21)
			return false;

		return setQulity(q);
		
	}


	return false;
}

//=======================================================================
bool CLArecontPanoramicDevice::setResolution(bool full)
{
	int value = full ? 15 : 0; // all sensors to full/half resolution

	if (CL_HTTP_SUCCESS!=setRegister(3, 0xD1, value)) // FULL RES QULITY
		return false;
	
	return true;
}

bool CLArecontPanoramicDevice::setQulity(int q)
{
	if (CL_HTTP_SUCCESS!=setRegister(3, 0xED, q)) // FULL RES QULITY
		return false;


	if (CL_HTTP_SUCCESS!=setRegister(3, 0xEE, q)) // HALF RES QULITY
		return false;

}