#ifdef ENABLE_ONVIF

#include "multisensor_data_provider.h"

#include <utils/common/sleep.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/mserver_resource_discovery_manager.h>
#include <core/resource/resource_factory.h>
#include <nx/vms/server/resource/camera.h>
#include <nx/vms/api/data/resource_data.h>
#include <core/resource/security_cam_resource.h>

namespace
{
    const quint16 kStreamOpenWaitingTimeMs = 40000;
    const quint16 kSingleWaitingIterationMs = 20;
}

namespace nx {
namespace plugins {
namespace utils {

MultisensorDataProvider::MultisensorDataProvider(
    const nx::vms::server::resource::CameraPtr& resource)
    :
    CLServerPushStreamReader(resource),
    m_cameraResource(resource)
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

        auto reader = resource->createLiveDataProvider();
        QnAbstractStreamDataProviderPtr source(reader);
        auto liveStreamReader = dynamic_cast<QnLiveStreamProvider*> (reader);
        if (!liveStreamReader)
            CameraDiagnostics::LiveVideoIsNotSupportedResult();

        liveStreamReader->setDoNotConfigureCamera(!configureSensor);
        liveStreamReader->setRole(getRole());
        if (getRole() != Qn::CR_SecondaryLiveVideo)
            liveStreamReader->setPrimaryStreamParams(params);

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

QnSecurityCamResourcePtr MultisensorDataProvider::initSubChannelResource(quint32 channelNumber)
{
    QUrl url(m_cameraResource->getUrl());

    QUrlQuery urlQuery(url);
    urlQuery.addQueryItem(
        lit("channel"),
        QString::number(channelNumber));

    url.setQuery(urlQuery);

    const auto typeId = m_cameraResource->getTypeId();
    auto resourceFactory = m_resource->commonModule()->resourceDiscoveryManager();
    QnResourceParams params(QnUuid::createUuid(), url.toString(), m_cameraResource->getVendor());
    auto resource = resourceFactory->createResource(typeId, params)
        .dynamicCast<nx::vms::server::resource::Camera>();
    resource->setCommonModule(m_resource->commonModule());
    resource->setForceUseLocalProperties(true);
    resource->setRole(nx::vms::server::resource::Camera::Role::subchannel);

    resource->setId(params.resID);
    resource->setUrl(params.url);
    resource->setVendor(params.vendor);
    resource->setMAC(m_cameraResource->getMAC());;
    resource->setModel(m_cameraResource->getModel());
    resource->setAuth(m_cameraResource->getAuth());
    resource->setName(
        QString("channel-%1").arg(channelNumber));

    for (const auto& p: m_cameraResource->getRuntimeProperties())
        resource->setProperty(p.name, p.value);

    NX_VERBOSE(this, "Created subchannel resource with id [%1]", resource->getId());
    return resource;
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
