#include "av_resource.h"
#include "av_panoramic.h"
#include "../dataprovider/panoramic_cpul_tftp_dataprovider.h"
#include "core/resource/resource_media_layout.h"



QnArecontPanoramicResource::QnArecontPanoramicResource(const QString& name)
{
    setName(name);
}

bool QnArecontPanoramicResource::hasDualStreaming() const
{
    return false;
}

bool QnArecontPanoramicResource::getDescription()
{
    m_description = "";
    return true;
}

QnAbstractStreamDataProvider* QnArecontPanoramicResource::createLiveDataProvider()
{
    cl_log.log("Creating streamreader for ", getHostAddress().toString(), cl_logDEBUG1);
    return new AVPanoramicClientPullSSTFTPStreamreader(toSharedPointer());
}


bool QnArecontPanoramicResource::setParamPhysical(const QnParam &param, const QVariant& val )
{
    if (param.netHelper().isEmpty()) // check if we have paramNetHelper command for this param
        return false;

    if (param.type()==QnParamType::None || param.type()==QnParamType::Button)
    {
        CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

        QString request = QLatin1String("set?") + param.netHelper();

        if (connection.doGET(request)!=CL_HTTP_SUCCESS)
            if (connection.doGET(request)!=CL_HTTP_SUCCESS) // try twice.
                return false;
    }
    else
    {
        for (int i = 1; i <=4 ; ++i)
        {
            CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

            QString request = QLatin1String("set") + QString::number(i) + QLatin1Char('?') + param.netHelper() + QLatin1Char('=') + val.toString();

            if (connection.doGET(request)!=CL_HTTP_SUCCESS)
                if (connection.doGET(request)!=CL_HTTP_SUCCESS) // try twice.
                    return false;
        }
    }

    return true;
}

bool QnArecontPanoramicResource::setSpecialParam(const QString& name, const QVariant& val, QnDomain domain)
{
    Q_UNUSED(domain);

    if (name == QLatin1String("resolution"))
    {
        if (val.toString() == QLatin1String("half"))
            return setResolution(false);
        if (val.toString() == QLatin1String("full"))
            return setResolution(true);
    }
    else if (name == QLatin1String("Quality"))
    {
        int q = val.toInt();
        if (q >= 1 && q <= 21)
            return setCamQuality(q);
    }

    return false;
}

void QnArecontPanoramicResource::init()
{
    QnPlAreconVisionResource::init();
    setRegister(3, 100, 10); // sets I frame frequency to 10
}

//=======================================================================
bool QnArecontPanoramicResource::setResolution(bool full)
{
    int value = full ? 15 : 0; // all sensors to full/half resolution

    if (CL_HTTP_SUCCESS!=setRegister(3, 0xD1, value)) // FULL RES QULITY
        return false;

    return true;
}

bool QnArecontPanoramicResource::setCamQuality(int q)
{
    if (CL_HTTP_SUCCESS!=setRegister(3, 0xED, q)) // FULL RES QULITY
        return false;

    if (CL_HTTP_SUCCESS!=setRegister(3, 0xEE, q)) // HALF RES QULITY
        return false;

    return false;

}

