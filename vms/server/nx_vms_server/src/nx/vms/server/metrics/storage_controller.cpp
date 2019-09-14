#include "storage_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

namespace {

class StorageDescription: public utils::metrics::ResourceDescription<QnStorageResource*>
{
public:
    using base = utils::metrics::ResourceDescription<QnStorageResource*>;
    using base::base;

    QString id() const override { return this->resource->getId().toSimpleString(); }
    utils::metrics::Scope scope() const override { return utils::metrics::Scope::local; }
};

} // namespace

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
                    add(std::make_unique<StorageDescription>(storage.get()));
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
    static const std::chrono::seconds kIoRateUpdateInterval(5);
    static auto ioRate = [](const auto& r, const auto& metric)
    {
        const auto bytes = r->getAndResetMetric(metric);
        const auto kBps = round(bytes / 1000.0 / kIoRateUpdateInterval.count());
        return StorageController::Value(kBps);
    };

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
                "type", [](const auto& r) { return Value(r->getStorageType()); }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "state",
            utils::metrics::makeSystemValueProvider<Resource>(
                "status", // FIXME: Impl does not fallow spec so far.
                [](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
                qtSignalWatch<Resource>(&QnStorageResource::statusChanged)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "issues", [](const auto&) { return Value(7); } // TODO: Implement.
            )
        ),
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            "activity",
            utils::metrics::makeLocalValueProvider<Resource>(
                "inRate", //< KB/s.
                [](const auto& r) { return ioRate(r, &QnStorageResource::Metrics::bytesRead); },
                nx::vms::server::metrics::timerWatch<QnStorageResource*>(kIoRateUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "outRate", //< KB/s.
                [](const auto& r) { return ioRate(r, &QnStorageResource::Metrics::bytesWritten); },
                nx::vms::server::metrics::timerWatch<QnStorageResource*>(kIoRateUpdateInterval)
                )
        )
        // TODO: Add Space groups.
    );

}

} // namespace nx::vms::server::metrics



