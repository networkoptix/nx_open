#include "camera_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

CameraController::CameraController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<resource::Camera*>("cameras", makeProviders())
{
}

void CameraController::start()
{
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>())
            {
                // TODO: Monitor for camera transfers to the different server.
                if (camera->getParentId() == serverModule()->commonModule()->moduleGUID())
                    add(std::make_unique<ResourceDescription<Resource>>(camera.get()));
            }
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>())
                remove(camera->getId().toSimpleString());
        });
}

utils::metrics::ValueGroupProviders<CameraController::Resource> CameraController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            "info",
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "name", [](const auto& r) { return Value(r->getName()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "server", [](const auto& r) { return Value(r->getParentId().toSimpleString()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "type", [](const auto&) { return Value(); } // TODO: Get actual type.
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "ip", [](const auto& r) { return Value(r->getUrl()); } // TODO: Get host from this URL.
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "vendor", [](const auto& r) { return Value(r->getVendor()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "model", [](const auto& r) { return Value(r->getModel()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "firmware", [](const auto& r) { return Value(r->getFirmware()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "status",
                [](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
                qtSignalWatch<Resource>(&resource::Camera::statusChanged)
            )
            // TODO: Implement params.
        )
        // TODO: Implement other groups.
    );
}

} // namespace nx::vms::server::metrics
