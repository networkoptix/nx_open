#ifdef ENABLE_ONVIF

#include "multisensor_data_provider.h"
#include "isolated_stream_reader_resource.h"

#include <plugins/resource/onvif/onvif_resource.h>
#include <plugins/resource/onvif/onvif_stream_reader.h>
#include <utils/common/sleep.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <common/static_common_module.h>

namespace
{
    const int kDefaultReceiveTimout = 30;
    const int kDefaultSendTimeout = 30;
    const quint16 kStreamOpenWaitingTimeMs = 40000;
    const quint16 kSingleWaitingIterationMs = 20;
}

namespace nx {
namespace plugins {
namespace utils {

MultisensorDataProvider::MultisensorDataProvider(const QnResourcePtr& res) :
    CLServerPushStreamReader(res),
    m_onvifRes(res.dynamicCast<QnPlOnvifResource>())
{

}

MultisensorDataProvider::~MultisensorDataProvider()
{
    m_dataSource.proxyCloseStream();
    pleaseStop();
    wait();
}

QnAbstractMediaDataPtr MultisensorDataProvider::getNextData()
{
    if (needMetadata())
        return getMetadata();

    auto data = m_dataSource.retrieveData();

    return data;
}

void MultisensorDataProvider::closeStream()
{
    m_dataSource.proxyCloseStream();
    m_dataSource.setUser(nullptr);
}

bool MultisensorDataProvider::isStreamOpened() const
{
    return  m_dataSource.isStreamOpened();
}

CameraDiagnostics::Result MultisensorDataProvider::openStreamInternal(
    bool isCameraControlRequired,
    const QnLiveStreamParams& params)
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    m_dataSource.resetSources();

    auto videoChannelMapping = getVideoChannelMapping();

    auto resData = qnStaticCommon->dataPool()->data(
        m_resource.dynamicCast<QnSecurityCamResource>());

    bool doNotConfigureCamera = resData.value<bool>(
        Qn::DO_NOT_CONFIGURE_CAMERA_PARAM_NAME, false);

    for (const auto& resourceChannelMapping: videoChannelMapping)
    {
        auto resource = initSubChannelResource(
            resourceChannelMapping.resourceChannel);

        resource->setOnvifRequestsRecieveTimeout(kDefaultReceiveTimout);
        resource->setOnvifRequestsSendTimeout(kDefaultSendTimeout);

        auto reader = new QnOnvifStreamReader(resource);
        reader->setMustNotConfigureResource(doNotConfigureCamera);

        QnAbstractStreamDataProviderPtr source(reader);
        if (!doNotConfigureCamera)
            reader->setParams(params);

        reader->setRole(getRole());

        doNotConfigureCamera = true;
        m_dataSource.addDataSource(source);

        for (const auto& channelMapping: resourceChannelMapping.channelMap)
        {
            for(const auto& mappedChannel: channelMapping.mappedChannels)
            {
                m_dataSource.mapSourceVideoChannel(
                    source.data(),
                    channelMapping.originalChannel,
                    mappedChannel);
            }
        }
    }

    //Need to do something with audio channel mapping.
    //At this point I even don't know how Pelco's cameras transmit audio

    m_dataSource.setUser(this);
    m_dataSource.proxyOpenStream(isCameraControlRequired, params);

    auto triesLeft = kStreamOpenWaitingTimeMs / kSingleWaitingIterationMs;
    while (!m_dataSource.isStreamOpened() && triesLeft-- && !m_needStop)
        QnSleep::msleep(kSingleWaitingIterationMs);

    return CameraDiagnostics::NoErrorResult();
}

void MultisensorDataProvider::pleaseStop()
{
    closeStream();
    m_needStop = true;
}

QnPlOnvifResourcePtr MultisensorDataProvider::initSubChannelResource(quint32 channelNumber)
{
    QUrl url(m_onvifRes->getUrl());
    QUrl onvifUrl = url;

    QUrlQuery urlQuery(url);
    urlQuery.addQueryItem(
        lit("channel"),
        QString::number(channelNumber));

    url.setQuery(urlQuery);

    QnPlOnvifResourcePtr subChannelResource(
        new nx::plugins::utils::IsolatedStreamReaderResource(m_resource->commonModule()));

    subChannelResource->setId(QnUuid::createUuid());
    subChannelResource->setTypeId(m_onvifRes->getTypeId());
    subChannelResource->setMAC(m_onvifRes->getMAC());;

    subChannelResource->setUrl(url.toString());
    subChannelResource->setDeviceOnvifUrl(onvifUrl.toString());
    subChannelResource->setModel(m_onvifRes->getModel());
    subChannelResource->setVendor(m_onvifRes->getVendor());
    subChannelResource->setAuth(m_onvifRes->getAuth());

    subChannelResource->setName(
        QString("channel-%1").arg(channelNumber));

    return subChannelResource;
}

QList<QnResourceChannelMapping> MultisensorDataProvider::getVideoChannelMapping()
{
    auto secRes = m_resource.dynamicCast<QnSecurityCamResource>();

    auto resData = qnStaticCommon->dataPool()->data(
        secRes->getVendor(),
        secRes->getModel());

    return resData.value<QList<QnResourceChannelMapping>>(
        Qn::VIDEO_MULTIRESOURCE_CHANNEL_MAPPING_PARAM_NAME);
}

} // namespace utils
} // namespace plugins
} // namespace nx

#endif // ENABLE_ONVIF
