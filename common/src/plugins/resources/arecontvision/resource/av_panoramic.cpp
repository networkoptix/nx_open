#include "av_resource.h"
#include "av_panoramic.h"
#include "../dataprovider/panoramic_cpul_tftp_dataprovider.h"
#include "core/resource/resource_media_layout.h"


class AVVideoLayout180 : public QnVideoResourceLayout
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

    virtual unsigned int v_position(unsigned int /*channel*/) const
    {
        return 0;
    }

};

class AVVideoLayout360 : public QnVideoResourceLayout
{
public:
    AVVideoLayout360(){};
    virtual ~AVVideoLayout360(){};
    //returns number of video channels device has

    virtual unsigned int numberOfChannels() const
    {
        return 4;
    }

    virtual unsigned int numberOfAudioChannels() const
    {
        return 1;
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

    virtual unsigned int v_position(unsigned int /*channel*/) const
    {
        return 0;
    }

};

AVVideoLayout360 avVideoLayout360;
AVVideoLayout180 avVideoLayout180;



CLArecontPanoramicResource::CLArecontPanoramicResource(const QString& name)
{
    setName(name);
    if (name.contains("8180") || name.contains("8185"))
        m_vrl = &avVideoLayout180;
    else
        m_vrl = &avVideoLayout360;
}


bool CLArecontPanoramicResource::getDescription()
{
	m_description = "";
	return true;
}

QnAbstractStreamDataProvider* CLArecontPanoramicResource::createLiveDataProvider()
{
	cl_log.log("Creating streamreader for ", getHostAddress().toString(), cl_logDEBUG1);
	return new AVPanoramicClientPullSSTFTPStreamreader(toSharedPointer());
}


bool CLArecontPanoramicResource::setParamPhysical(const QString& name, const QnValue& val )
{
    QnParam& param = getResourceParamList().get(name);

    if (param.netHelper().isEmpty()) // check if we have paramNetHelper command for this param
        return false;

	if (param.type()==QnParamType::None || param.type()==QnParamType::Button) 
	{
		CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());
		QString request;

		QTextStream str(&request);
		str << "set?" << param.netHelper();

        if (connection.doGET(request)!=CL_HTTP_SUCCESS)
            if (connection.doGET(request)!=CL_HTTP_SUCCESS) // try twice.
				return false;
	}
	else
	{
		for (int i = 1; i <=4 ; ++i)
		{
			CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());
			QString request;

			QTextStream str(&request);
			str << "set" << i << "?" << param.netHelper();
			str << "=" << (QString)val;

            if (connection.doGET(request)!=CL_HTTP_SUCCESS)
                if (connection.doGET(request)!=CL_HTTP_SUCCESS) // try twice.
					return false;
		}

	}

	return true;

}

bool CLArecontPanoramicResource::setSpecialParam(const QString& name, const QnValue& val, QnDomain domain)
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
bool CLArecontPanoramicResource::setResolution(bool full)
{
	int value = full ? 15 : 0; // all sensors to full/half resolution

	if (CL_HTTP_SUCCESS!=setRegister(3, 0xD1, value)) // FULL RES QULITY
		return false;

	return true;
}

bool CLArecontPanoramicResource::setCamQulity(int q)
{
	if (CL_HTTP_SUCCESS!=setRegister(3, 0xED, q)) // FULL RES QULITY
		return false;

	if (CL_HTTP_SUCCESS!=setRegister(3, 0xEE, q)) // HALF RES QULITY
		return false;

	return false;

}

const QnVideoResourceLayout* CLArecontPanoramicResource::getVideoLayout(const QnAbstractMediaStreamDataProvider* /*dataProvider*/)
{
    return m_vrl;
}
