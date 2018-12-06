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

QnArecontPanoramicResource::QnArecontPanoramicResource(
    QnMediaServerModule* serverModule, const QString& name)
    :
    QnPlAreconVisionResource(serverModule)
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
        NX_DEBUG(this, lit("Arecont panoramic. Creating live RTSP provider for camera %1").arg(getHostAddress()));
        return new QnArecontRtspStreamReader(toSharedPointer(this));
    }
    else
    {
        NX_DEBUG(this, lit("Create live provider for camera %1").arg(getHostAddress()));
        return new AVPanoramicClientPullSSTFTPStreamreader(toSharedPointer(this));
    }
}

bool QnArecontPanoramicResource::getParamPhysicalByChannel(int channel, const QString& name,
    QString& val)
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

bool QnArecontPanoramicResource::setApiParameter(const QString& id, const QString& value)
{
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

bool QnArecontPanoramicResource::setSpecialParam(const QString& id, const QString& value)
{
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

nx::vms::server::resource::StreamCapabilityMap QnArecontPanoramicResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex streamIndex)
{
    // TODO: implement me
    return nx::vms::server::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result QnArecontPanoramicResource::initializeCameraDriver()
{
    const CameraDiagnostics::Result result = QnPlAreconVisionResource::initializeCameraDriver();
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    setRegister(3, 100, 10); // sets I frame frequency to 10

    setApiParameter(lit("cnannelenable"), QString::number(15)); // to enable all channels

    getVideoLayout(0);

    return CameraDiagnostics::NoErrorResult();
}

void QnArecontPanoramicResource::updateFlipState()
{
    QUrl devUrl(getUrl());
    CLSimpleHTTPClient connection(
        getHostAddress(),
        devUrl.port(nx::network::http::DEFAULT_HTTP_PORT),
        getNetworkTimeout(),
        getAuth());

    QString request = QLatin1String("get?rotate");
    CLHttpStatus responseCode = connection.doGET(request);
    if (responseCode != CL_HTTP_SUCCESS)
        return;

    QByteArray response;
    connection.readAll(response);
    int valPos = response.indexOf('=');
    if (valPos >= 0)
        m_isRotated = response.mid(valPos+1).trimmed().toInt();

    auto layout = QnArecontPanoramicResource::getDefaultVideoLayout();
    if (!layout)
        return;

    QString newVideoLayoutString;
    {
        QnMutexLocker lock(&m_layoutMutex);
        m_customVideoLayout.reset(new QnCustomResourceVideoLayout(layout->size()));
        QVector<int> channels = layout->getChannels();
        if (m_isRotated)
            std::reverse(channels.begin(), channels.end());
        m_customVideoLayout->setChannels(channels);
        newVideoLayoutString = m_customVideoLayout->toString();
    }

    // Get from kvpairs directly. Do not read default value from resourceTypes.
    QString oldVideoLayoutString = commonModule()
        ->propertyDictionary()
        ->value(getId(), ResourcePropertyKey::kVideoLayout);
    if (newVideoLayoutString != oldVideoLayoutString)
    {
        commonModule()
            ->propertyDictionary()
            ->setValue(getId(), ResourcePropertyKey::kVideoLayout, newVideoLayoutString);
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
    {
        m_defaultVideoLayout = QnResourceVideoLayoutPtr(QnCustomResourceVideoLayout::fromString(
            resType->defaultValue(ResourcePropertyKey::kVideoLayout)));
    }
    else
    {
        m_defaultVideoLayout = QnResourceVideoLayoutPtr(new QnDefaultResourceVideoLayout());
    }
    return m_defaultVideoLayout;
}

int QnArecontPanoramicResource::getChannelCount() const
{
    auto layout = getVideoLayout(nullptr);
    if (!layout)
        return kDefaultChannelCount;

    return layout->channelCount();
}

void QnArecontPanoramicResource::initializeVideoLayoutUnsafe() const
{
    auto layoutString = getProperty(ResourcePropertyKey::kVideoLayout);
    m_customVideoLayout = QnCustomResourceVideoLayout::fromString(layoutString);
}

QnConstResourceVideoLayoutPtr QnArecontPanoramicResource::getVideoLayout(
    const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    // Saving id before locking m_layoutMutex to avoid potential deadlock.
    const auto resourceId = getId();

    QnMutexLocker lock(&m_layoutMutex);
    if (!m_customVideoLayout)
        initializeVideoLayoutUnsafe();
    return m_customVideoLayout;
}

#endif
