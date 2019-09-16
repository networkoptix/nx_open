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
    const auto currentServerId = serverModule()->commonModule()->moduleGUID();
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this, currentServerId](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>())
            {
                // TODO: Monitor for camera transfers to the different server. If it happens,
                // resource should be removed and added with appropriate scope marker.
                add(camera.get(), camera->getPhysicalId(), (camera->getParentId() == moduleGUID())
                    ? utils::metrics::Scope::local
                    : utils::metrics::Scope::system);
            }
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>())
                remove(camera->getId());
        });
}

utils::metrics::ValueGroupProviders<CameraController::Resource> CameraController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->getName()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "server", [](const auto& r) { return Value(r->getParentId().toSimpleString()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "type", [](const auto&) { return Value(); } // TODO: Get actual type.
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "ip", [](const auto& r) { return Value(r->getUrl()); } // TODO: Get host from this URL.
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "vendor", [](const auto& r) { return Value(r->getVendor()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "model", [](const auto& r) { return Value(r->getModel()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "firmware", [](const auto& r) { return Value(r->getFirmware()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
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
