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
            api::metrics::Label{
                "info", "Info"
            },
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"name", "Name", "table&panel", ""},
                [](const auto& r) { return Value(r->getName()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"server", "Server", "table&panel", ""},
                [](const auto& r) { return Value(r->getParentId().toSimpleString()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"type", "Type", "table&panel", ""},
                [](const auto&) { return Value(); } // TODO: Get actual status.
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"ip", "IP", "table&panel", ""},
                [](const auto& r) { return Value(r->getUrl()); } // TODO: Get host from this URL.
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"vendor", "Vendor", "panel", ""},
                [](const auto& r) { return Value(r->getVendor()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"model", "Model", "panel", ""},
                [](const auto& r) { return Value(r->getModel()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"firmware", "Firmware", "panel", ""},
                [](const auto& r) { return Value(r->getFirmware()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"status", "Status", "table&panel", ""},
                [](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
                qtSignalWatch<Resource>(&resource::Camera::statusChanged)
            )
            // TODO: Implement params.
        )
        // TODO: Implement other groups.
    );
}

} // namespace nx::vms::server::metrics
