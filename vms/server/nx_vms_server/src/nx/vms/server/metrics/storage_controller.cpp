#include "storage_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

StorageController::StorageController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<StorageResource*>("storages", makeProviders())
{
}

void StorageController::start()
{
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            // TODO: Consider to add system storages as well.
            if (const auto storage = resource.dynamicCast<StorageResource>();
                storage && storage->getParentId() == serverModule()->commonModule()->moduleGUID())
            {
                add(storage.get(), storage->getId(), utils::metrics::Scope::local);
            }
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (const auto storage = resource.dynamicCast<QnStorageResource>();
                storage && storage->getParentId() == serverModule()->commonModule()->moduleGUID())
            {
                remove(storage->getId());
            }
        });
}

double roundOff(double value, int digits = 2)
{
    int k = 1;
    for (int i = 0; i < digits; ++i)
        k *= 10;
    return round(value * k) / k;
}

utils::metrics::ValueGroupProviders<StorageController::Resource> StorageController::makeProviders()
{
    static const std::chrono::seconds kIoRateUpdateInterval(5);
    static auto ioRate =
        [](const auto& r, const auto& metric)
        {
            const auto bytes = r->getAndResetMetric(metric);
            const auto kBps = roundOff(bytes / 1000.0 / kIoRateUpdateInterval.count());
            return StorageController::Value(kBps);
        };

    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->getPath()); }
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
                "readRateKBps",
                [](const auto& r) { return ioRate(r, &StorageResource::Metrics::bytesRead); },
                nx::vms::server::metrics::timerWatch<QnStorageResource*>(kIoRateUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "writeRateKBps",
                [](const auto& r) { return ioRate(r, &StorageResource::Metrics::bytesWritten); },
                nx::vms::server::metrics::timerWatch<QnStorageResource*>(kIoRateUpdateInterval)
            )
        ),
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            "space",
            utils::metrics::makeLocalValueProvider<Resource>(
                "totalSpaceGb",
                [](const auto& r) { return roundOff(r->getTotalSpace() / 1000000000.0); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "mediaSpaceInPercents",
                [](const auto& r)
                { return roundOff(r->nxOccupedSpace() / (double) r->getTotalSpace() * 100); }
            )
        )
    );

}

} // namespace nx::vms::server::metrics



