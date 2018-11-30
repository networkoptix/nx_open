#ifdef ENABLE_ONVIF

#include "multisensor_data_provider.h"
#include "isolated_stream_reader_resource.h"

#include <plugins/resource/onvif/onvif_resource.h>
#include <plugins/resource/onvif/onvif_stream_reader.h>
#include <utils/common/sleep.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>

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

MultisensorDataProvider::MultisensorDataProvider(
    const QnPlOnvifResourcePtr& res)
    :
    CLServerPushStreamReader(res),
    m_onvifRes(res)
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

    auto resData = m_resource.dynamicCast<QnSecurityCamResource>()->resourceData();
    const auto configureEachSensor = resData.value<bool>(
        ResourceDataKey::kConfigureAllStitchedSensors, false);

    bool configureSensor = true;
    for (const auto& resourceChannelMapping: videoChannelMapping)
    {
        auto resource = initSubChannelResource(
            resourceChannelMapping.resourceChannel);

        resource->setOnvifRequestsRecieveTimeout(kDefaultReceiveTimout);
        resource->setOnvifRequestsSendTimeout(kDefaultSendTimeout);

        auto reader = new QnOnvifStreamReader(resource);
        reader->setMustNotConfigureResource(!configureSensor);

        QnAbstractStreamDataProviderPtr source(reader);
        if (getRole() == Qn::CR_LiveVideo)
            reader->setPrimaryStreamParams(params);

        reader->setRole(getRole());

        configureSensor = configureEachSensor;
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
        new nx::plugins::utils::IsolatedStreamReaderResource(serverModule()));

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

    auto resData = m_resource->commonModule()->dataPool()->data(
        secRes->getVendor(),
        secRes->getModel());

    return resData.value<QList<QnResourceChannelMapping>>(
        ResourceDataKey::kMultiresourceVideoChannelMapping);
}

} // namespace utils
} // namespace plugins
} // namespace nx

#endif // ENABLE_ONVIF
