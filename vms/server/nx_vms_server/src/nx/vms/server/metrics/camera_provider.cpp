#include "camera_provider.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

CameraProvider::CameraProvider(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceProvider<resource::Camera*>(makeProviders())
{
}

void CameraProvider::startMonitoring()
{
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>().get())
            {
                found(camera);
                QObject::connect(
                    camera, &QnResource::flagsChanged, this,
                    [this, camera](QnResourcePtr) { changed(camera); });
            }
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>().get())
            {
                camera->disconnect(this);
                lost(camera);
            }
        });
}

std::optional<utils::metrics::ResourceDescription> CameraProvider::describe(
    const Resource& resource) const
{
    if (resource->flags().testFlag(Qn::foreigner))
        return std::nullopt;

    return utils::metrics::ResourceDescription(
        resource->getId().toSimpleString(),
        resource->getParentId().toSimpleString(),
        resource->getName());
}

CameraProvider::ParameterProviders CameraProvider::makeProviders()
{
    return parameterProviders(
        singleParameterProvider(
            {"url", "URL"},
            [](const auto& resource) { return Value(resource->getUrl()); },
            qtSignalWatch<Resource>(&QnResource::urlChanged)
        ),
        singleParameterProvider(
            {"type"},
            [](const auto& /*resource*/) { return Value("camera"); }, //< TODO: isEncoder().
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"vendor"},
            [](const auto& resource) { return Value(resource->getVendor()); },
            qtSignalWatch<Resource>(&QnSecurityCamResource::vendorChanged)
        ),
        singleParameterProvider(
            {"model"},
            [](const auto& resource) { return Value(resource->getModel()); },
            qtSignalWatch<Resource>(&QnSecurityCamResource::modelChanged)
        ),
        singleParameterProvider(
            {"firmware"},
            [](const auto& resource) { return Value(resource->getFirmware()); },
            propertyWatch<Resource>(ResourcePropertyKey::kFirmware)
        ),
        singleParameterProvider(
            {"status"},
            [](const auto& resource) { return Value(QnLexical::serialized(resource->getStatus())); },
            qtSignalWatch<Resource>(&QnResource::statusChanged)
        ),
        singleParameterProvider(
            {"packetLossCount", "number of packets lost in 1h"},
            [](const auto& /*resource*/) { return Value(0); }, //< TODO: Implement.
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"conflictCount", "number of IP conflicts in 3m"},
            [](const auto& /*resource*/) { return Value(0); }, //< TODO: Implement.
            staticWatch<Resource>()
        ),
        makeStreamProvider(api::StreamIndex::primary),
        makeStreamProvider(api::StreamIndex::secondary),
        makeStorageProvider()
    );
}

CameraProvider::ParameterProviderPtr CameraProvider::makeStreamProvider(api::StreamIndex index)
{
    // TODO: Implement actual providers for each video stream.
    return parameterGroupProvider({
            (index == api::StreamIndex::primary) ? "primaryStream" : "secondaryStream",
            (index == api::StreamIndex::primary) ? "primary stream" : "secondary stream"
        },
        singleParameterProvider(
            {"resolution"},
            [](const auto& /*resource*/) { return Value("800x600"); },
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"targetFps", "target FPS", "fps"},
            [](const auto& /*resource*/) { return Value(30); },
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"actualFps", "actual FPS", "fps"},
            [](const auto& /*resource*/) { return Value(27); },
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"bitrate", "bitrate", "bps"},
            [](const auto& /*resource*/) { return Value(2500000); },
            staticWatch<Resource>()
        )
    );
}

CameraProvider::ParameterProviderPtr CameraProvider::makeStorageProvider()
{
    // TODO: Implement actual providers.
    return parameterGroupProvider({"storage"},
        singleParameterProvider(
            {"bitrate", "bitrate", "bps"},
            [](const auto& /*resource*/) { return Value(2500000); },
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"retention", "", "days"},
            [](const auto& /*resource*/) { return Value(300); },
            staticWatch<Resource>()
        )
    );
}

} // namespace nx::vms::server::metrics
