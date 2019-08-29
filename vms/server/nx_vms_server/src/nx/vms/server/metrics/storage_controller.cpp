#include "storage_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

StorageController::StorageController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<QnStorageResource*>("storages", makeProviders())
{
}

void StorageController::start()
{
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (const auto storage = resource.dynamicCast<QnStorageResource>())
            {
                if (storage->getParentId() == serverModule()->commonModule()->moduleGUID())
                    add(std::make_unique<ResourceDescription<Resource>>(storage.get()));
            }
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (const auto storage = resource.dynamicCast<QnStorageResource>())
                remove(storage->getId().toSimpleString());
        });
}

utils::metrics::ValueGroupProviders<StorageController::Resource> StorageController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            api::metrics::Label{
                "info", "Info"
            },
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"type", "Type", "table&panel", ""},
                [](const auto& r) { return Value(r->getStorageType()); }
            )
        ),
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            api::metrics::Label{
                "state", "State"
            },
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"status", "Status", "table&panel", ""},
                // FIXME: Impl does not fallow spec so far.
                [](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
                qtSignalWatch<Resource>(&QnStorageResource::statusChanged)
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"issues", "Issues (24h)", "table&panel", ""},
                [](const auto&) { return Value(); } // TODO: Implement.
            )
        )
        // TODO: Add Activity and Space groups.
    );
}

} // namespace nx::vms::server::metrics



