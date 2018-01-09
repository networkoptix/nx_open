#ifdef ENABLE_ARECONT

#include <nx/utils/log/log.h>
#include <nx/network/http/http_types.h>

#include "../dataprovider/av_rtsp_stream_reader.h"
#include "../dataprovider/panoramic_cpul_tftp_dataprovider.h"
#include "core/resource/resource_media_layout.h"

#include "av_resource.h"
#include "av_panoramic.h"
#include "core/resource_management/resource_properties.h"
#include "core/resource/resource.h"
#include <common/common_module.h>

#define MAX_RESPONSE_LEN (4*1024)

namespace {

const int kDefaultChannelCount = 4;

} // namespace


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
    if (isRTSPSupported())
    {
        NX_LOG(lit("Arecont panoramic. Creating live RTSP provider for camera %1").arg(getHostAddress()), cl_logDEBUG1);
        return new QnArecontRtspStreamReader(toSharedPointer());
    }
    else
    {
        NX_LOG( lit("Create live provider for camera %1").arg(getHostAddress()), cl_logDEBUG1);
        return new AVPanoramicClientPullSSTFTPStreamreader(toSharedPointer());
    }
}

bool QnArecontPanoramicResource::getParamPhysicalByChannel(int channel, const QString& name, QString &val)
{
    m_mutex.lock();
    m_mutex.unlock();
    QUrl devUrl(getUrl());
    CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());
    QString request = QLatin1String("get") + QString::number(channel) + QLatin1String("?") + name;

    CLHttpStatus status = connection.doGET(request);
    if (status == CL_HTTP_AUTH_REQUIRED)
        setStatus(Qn::Unauthorized);

    if (status != CL_HTTP_SUCCESS)
        return false;


    QByteArray response;
    connection.readAll(response);
    int index = response.indexOf('=');
    if (index==-1)
        return false;

    val = QLatin1String(response.mid(index+1));

    return true;
}

bool QnArecontPanoramicResource::setParamPhysical(const QString &id, const QString &value) {
    if (setSpecialParam(id, value))
        return true;
    QUrl devUrl(getUrl());
    auto channelCount = getChannelCount();

    for (int i = 1; i <= channelCount; ++i)
    {
        CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());

        QString request = QLatin1String("set") + QString::number(i) + QLatin1Char('?') + id + QLatin1Char('=') + value;

        if (connection.doGET(request)!=CL_HTTP_SUCCESS)
            if (connection.doGET(request)!=CL_HTTP_SUCCESS) // try twice.
                return false;
    }
    return true;
}

bool QnArecontPanoramicResource::setSpecialParam(const QString &id, const QString& value) {
    if (id == lit("resolution"))
    {
        if (value == lit("half"))
            return setResolution(false);
        if (value == lit("full"))
            return setResolution(true);
    }
    else if (id == lit("Quality"))
    {
        int q = value.toInt();
        if (q >= 1 && q <= 21)
            return setCamQuality(q);
    }

    return false;
}

CameraDiagnostics::Result QnArecontPanoramicResource::initInternal()
{
    nx::mediaserver::resource::Camera::initInternal();
    const CameraDiagnostics::Result result = QnPlAreconVisionResource::initInternal();
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    setRegister(3, 100, 10); // sets I frame frequency to 10

    setParamPhysical(lit("cnannelenable"), QString::number(15)); // to enable all channels

    getVideoLayout(0);

    return CameraDiagnostics::NoErrorResult();
}

void QnArecontPanoramicResource::updateFlipState()
{
    QUrl devUrl(getUrl());
    CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());
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

    auto layout = QnArecontPanoramicResource::getDefaultVideoLayout();
    if (!layout)
        return;

    {
        QnMutexLocker lock(&m_layoutMutex);
        m_customVideoLayout.reset(new QnCustomResourceVideoLayout(layout->size()));
        QVector<int> channels = layout->getChannels();
        if (m_isRotated)
            std::reverse(channels.begin(), channels.end());
        m_customVideoLayout->setChannels(channels);
    }

    QString oldVideoLayout = commonModule()->propertyDictionary()->value(getId(), Qn::VIDEO_LAYOUT_PARAM_NAME); // get from kvpairs directly. do not read default value from resourceTypes
    QString newVideoLayout = m_customVideoLayout->toString();
    if (newVideoLayout != oldVideoLayout) {
        commonModule()->propertyDictionary()->setValue(getId(), Qn::VIDEO_LAYOUT_PARAM_NAME, newVideoLayout);
        commonModule()->propertyDictionary()->saveParams(getId());
    }
}

// =======================================================================
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

QnConstResourceVideoLayoutPtr QnArecontPanoramicResource::getDefaultVideoLayout() const
{
    if (m_defaultVideoLayout)
        return m_defaultVideoLayout;

    QnResourceTypePtr resType = qnResTypePool->getResourceType(getTypeId());
    if (resType)
        m_defaultVideoLayout = QnResourceVideoLayoutPtr(QnCustomResourceVideoLayout::fromString(resType->defaultValue(Qn::VIDEO_LAYOUT_PARAM_NAME)));
    else
        m_defaultVideoLayout = QnResourceVideoLayoutPtr(new QnDefaultResourceVideoLayout());
    return m_defaultVideoLayout;
}

int QnArecontPanoramicResource::getChannelCount() const
{
    auto layout = getVideoLayout(nullptr);
    if (!layout)
        return kDefaultChannelCount;

    return layout->channelCount();
}

QnConstResourceVideoLayoutPtr QnArecontPanoramicResource::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    const auto resourceId = getId();    //saving id before locking m_layoutMutex to avoid potential deadlock

    Q_UNUSED(dataProvider)
    QnMutexLocker lock(&m_layoutMutex);
    return m_customVideoLayout ?
        m_customVideoLayout.dynamicCast<const QnResourceVideoLayout>() :
        getDefaultVideoLayout();
}

#endif
