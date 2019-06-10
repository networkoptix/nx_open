#include "camera_providers.h"

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::server::metrics {

CamerasProvider::CamerasProvider(QnResourcePool* resourcePool):
    utils::metrics::ResourceProvider<resource::CameraPtr>(makeProviders()),
    m_resourcePool(resourcePool)
{
}

void CamerasProvider::startMonitoring()
{
    QObject::connect(
        m_resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>())
            {
                found(camera);
                QObject::connect(
                    camera.get(), &QnResource::flagsChanged, this,
                    [this, camera](QnResourcePtr) { changed(camera); });
            }
        });

    QObject::connect(
        m_resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>())
            {
                camera->disconnect(this);
                lost(camera);
            }
        });
}

std::optional<utils::metrics::ResourceDescription> CamerasProvider::describe(
    const resource::CameraPtr& resource) const
{
    if (resource->flags().testFlag(Qn::foreigner))
        return std::nullopt;

    return utils::metrics::ResourceDescription{
        resource->getId().toSimpleString(),
        resource->getParentId().toSimpleString(),
        resource->getName()
    };
}

template<typename Resource = QnResource, typename Signal>
typename utils::metrics::SingleParameterProvider<QnSharedResourcePointer<Resource>>::Watch
    signalWatch(Signal signal)
{
    return [signal](
        const QnSharedResourcePointer<Resource>& resource,
        typename utils::metrics::SingleParameterProvider<Resource>::OnChange change)
    {
        const auto connection = QObject::connect(
            resource.get(), signal,
            [change = std::move(change)](const auto& /*resource*/) { change(); });

        return nx::utils::makeSharedGuard([connection]() { QObject::disconnect(connection); });
    };
}

CamerasProvider::ParameterProviders CamerasProvider::makeProviders()
{
    return parameterProviders(
        singleParameterProvider(
            {"url", "URL"},
            [](const auto& resource) { return Value(resource->getUrl()); },
            signalWatch<resource::Camera>(&QnResource::urlChanged)
        ),
        singleParameterProvider(
            {"type"},
            [](const auto& /*resource*/) { return Value("camera"); } //< TODO: isEncoder().
            // TODO: Detect changes.
        ),
        singleParameterProvider(
            {"vendor"},
            [](const auto& resource) { return Value(resource->getVendor()); }
            // TODO: Detect changes.
        ),
        singleParameterProvider(
            {"model"},
            [](const auto& resource) { return Value(resource->getModel()); }
            // TODO: Detect changes.
        ),
        singleParameterProvider(
            {"firmware"},
            [](const auto& resource) { return Value(resource->getFirmware()); }
            // TODO: Detect changes.
        ),
        singleParameterProvider(
            {"status"},
            [](const auto& resource) { return Value(QnLexical::serialized(resource->getStatus())); },
            signalWatch<resource::Camera>(&QnResource::statusChanged)
        ),
        singleParameterProvider(
            {"packetLossCount", "number of packets lost in 1h"},
            [](const auto& /*resource*/) { return Value(0); } //< TODO: Implement.
            // TODO: Detect changes.
        ),
        singleParameterProvider(
            {"conflictCount", "number of IP conflicts in 3m"},
            [](const auto& /*resource*/) { return Value(0); } //< TODO: Implement.
            // TODO: Detect changes.
        ),
        makeStreamProvider(api::StreamIndex::primary),
        makeStreamProvider(api::StreamIndex::secondary)
    );
}

CamerasProvider::ParameterProviderPtr CamerasProvider::makeStreamProvider(api::StreamIndex index)
{
    // TODO: Implement actual providers for each video stream.
    return parameterGroupProvider(
        {
            (index == api::StreamIndex::primary) ? "primaryStream" : "secondaryStream",
            (index == api::StreamIndex::primary) ? "primary stream" : "secondary stream"
        },
        singleParameterProvider(
            {"resolution"},
            [](const auto& /*resource*/) { return Value("800x600"); }
        ),
        singleParameterProvider(
            {"targetFps", "target FPS", "fps"},
            [](const auto& /*resource*/) { return Value(30); }
        ),
        singleParameterProvider(
            {"actualFps", "actual FPS", "fps"},
            [](const auto& /*resource*/) { return Value(28); }
        ),
        singleParameterProvider(
            {"bitrate", "bitrate", "bps"},
            [](const auto& /*resource*/) { return Value(2500000); }
        )
    );
}

} // namespace nx::vms::server::metrics
