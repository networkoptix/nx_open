#include "av_resource.h"
#include "av_panoramic.h"
#include "../dataprovider/panoramic_cpul_tftp_dataprovider.h"
#include "resource/media_resource_layout.h"

class AVVideoLayout180 : public QnMediaResourceLayout
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

class AVVideoLayout360 : public QnMediaResourceLayout
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
            return 3;

        case 2:
            return 2;

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

AVVideoLayout360 avVideoLayout360;
AVVideoLayout180 avVideoLayout180;

CLArecontPanoramicDevice::CLArecontPanoramicDevice(const QString& name):
m_hastestPattern(false)
{
    setName(name);
}

QnMediaResourceLayout* CLArecontPanoramicDevice::getVideoLayout() const
{
    QString name = getName();

    if (name.contains("8360") || name.contains("8365"))
        return &avVideoLayout360;
    else
        return &avVideoLayout180;
}

bool CLArecontPanoramicDevice::getDescription()
{
	m_description = "";
	return true;
}

QnAbstractMediaStreamDataProvider* CLArecontPanoramicDevice::getDeviceStreamConnection()
{
	cl_log.log("Creating streamreader for ", getHostAddress().toString(), cl_logDEBUG1);
	return new AVPanoramicClientPullSSTFTPStreamreader(this);
}

bool CLArecontPanoramicDevice::hasTestPattern() const
{
	return m_hastestPattern;
}

bool CLArecontPanoramicDevice::setParamPhysical(const QString& name, const QnValue& val )
{
    QnParam& param = getResourceParamList().get(name);

    if (param.paramNetHelper.isEmpty()) // check if we have paramNetHelper command for this param
        return false;

	if (param.type==QnParam::None || param.type==QnParam::Button) 
	{
		CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());
		QString request;

		QTextStream str(&request);
		str << "set?" << param.paramNetHelper;

		connection.setRequestLine(request);

		if (connection.openStream()!=CL_HTTP_SUCCESS)
			if (connection.openStream()!=CL_HTTP_SUCCESS) // try twice.
				return false;
	}
	else
	{
		for (int i = 1; i <=4 ; ++i)
		{
			CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());
			QString request;

			QTextStream str(&request);
			str << "set" << i << "?" << param.paramNetHelper;
			str << "=" << (QString)val;

			connection.setRequestLine(request);

			if (connection.openStream()!=CL_HTTP_SUCCESS)
				if (connection.openStream()!=CL_HTTP_SUCCESS) // try twice.
					return false;
		}

	}

	return true;

}

bool CLArecontPanoramicDevice::setSpecialParam(const QString& name, const QnValue& val, QnDomain domain)
{

	if (QnPlAreconVisionResource::setSpecialParam(name, val, domain))
		return true;

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

		return setCamQulity(q);

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

bool CLArecontPanoramicDevice::setCamQulity(int q)
{
	if (CL_HTTP_SUCCESS!=setRegister(3, 0xED, q)) // FULL RES QULITY
		return false;

	if (CL_HTTP_SUCCESS!=setRegister(3, 0xEE, q)) // HALF RES QULITY
		return false;

	return false;

}
