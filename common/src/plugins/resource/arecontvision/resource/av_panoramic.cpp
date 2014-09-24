#ifdef ENABLE_ARECONT

#include <utils/common/log.h>
#include <utils/network/http/httptypes.h>

#include "../dataprovider/panoramic_cpul_tftp_dataprovider.h"
#include "core/resource/resource_media_layout.h"

#include "av_resource.h"
#include "av_panoramic.h"

#define MAX_RESPONSE_LEN (4*1024)


QnArecontPanoramicResource::QnArecontPanoramicResource(const QString& name)
{
    setName(name);
    m_isRotated = false;
}

QnArecontPanoramicResource::~QnArecontPanoramicResource()
{
}


bool QnArecontPanoramicResource::getDescription()
{
    m_description = QString();
    return true;
}

QnAbstractStreamDataProvider* QnArecontPanoramicResource::createLiveDataProvider()
{
    NX_LOG( lit("Create live provider for camera %1").arg(getHostAddress()), cl_logDEBUG1);
    return new AVPanoramicClientPullSSTFTPStreamreader(toSharedPointer());
}


bool QnArecontPanoramicResource::getParamPhysical(int channel, const QString& name, QVariant &val)
{
    m_mutex.lock();
    if (!m_resourceParamList.contains(name))
    {
        m_mutex.unlock();
        return false;
    }

    QnParam param = m_resourceParamList[name];
    m_mutex.unlock();

    if (param.netHelper().isEmpty()) // check if we have paramNetHelper command for this param
        return false;

    CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

    

    QString request = QLatin1String("get") + QString::number(channel) + QLatin1String("?") + param.netHelper();

    CLHttpStatus status = connection.doGET(request);
    if (status == CL_HTTP_AUTH_REQUIRED)
        setStatus(Qn::Unauthorized);

    if (status != CL_HTTP_SUCCESS)
        return false;


    char c_response[MAX_RESPONSE_LEN];

    int result_size =  connection.read(c_response,sizeof(c_response));

    if (result_size <0)
        return false;

    QByteArray response = QByteArray::fromRawData(c_response, result_size); // QByteArray  will not copy data

    int index = response.indexOf('=');
    if (index==-1)
        return false;

    QByteArray rarray = response.mid(index+1);

    val = QLatin1String(rarray.data());

    return true;
}

bool QnArecontPanoramicResource::setParamPhysical(const QnParam &param, const QVariant& val )
{
    if (param.netHelper().isEmpty()) // check if we have paramNetHelper command for this param
        return false;

    if (param.type()==Qn::PDT_None || param.type()==Qn::PDT_Button)
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

CameraDiagnostics::Result QnArecontPanoramicResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();
    const CameraDiagnostics::Result result = QnPlAreconVisionResource::initInternal();
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    setRegister(3, 100, 10); // sets I frame frequency to 10

    setParam(QLatin1String("CnannelEnable"), 15, QnDomainPhysical); // to enable all channels

    updateFlipState();

    return CameraDiagnostics::NoErrorResult();
}

void QnArecontPanoramicResource::updateFlipState()
{
    if (m_flipTimer.isValid() && m_flipTimer.elapsed() < 1000)
        return;
    m_flipTimer.restart();

    CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());
    QString request = QLatin1String("get?rotate");
    CLHttpStatus responseCode = connection.doGET(request);
    if (responseCode != CL_HTTP_SUCCESS)
        return;

    QByteArray response;
    connection.readAll(response);
    int valPos = response.indexOf('=');
    if (valPos >= 0) {
        m_isRotated = response.mid(valPos+1).trimmed().toInt();
    }
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

QnConstResourceVideoLayoutPtr QnArecontPanoramicResource::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    QMutexLocker lock(&m_mutex);

    const QnConstResourceVideoLayoutPtr& layout = QnPlAreconVisionResource::getVideoLayout(dataProvider);
    //const QnConstCustomResourceVideoLayoutPtr& customLayout = std::dynamic_pointer_cast<const QnCustomResourceVideoLayout>(layout);
    const QnCustomResourceVideoLayout* customLayout = dynamic_cast<const QnCustomResourceVideoLayout*>(layout.data());
    const_cast<QnArecontPanoramicResource*>(this)->updateFlipState();
    if (m_isRotated && customLayout)
    {
        if (!m_rotatedLayout) {
            m_rotatedLayout.reset( new QnCustomResourceVideoLayout(customLayout->size()) );
            QVector<int> channels = customLayout->getChannels();
            std::reverse(channels.begin(), channels.end());
            m_rotatedLayout->setChannels(channels);
        }
        return m_rotatedLayout;
    }

    return layout;
}

#endif
