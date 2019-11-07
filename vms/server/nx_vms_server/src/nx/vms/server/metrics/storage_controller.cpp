#include "storage_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>

#include "helpers.h"
#include <core/resource/media_server_resource.h>
#include <nx/vms/server/analytics/db/analytics_db.h>

namespace nx::vms::server::metrics {

using namespace std::chrono;
using Value = StorageController::Value;
using Resource = StorageController::Resource;

static const std::chrono::seconds kIoRateUpdateInterval(5);
static const std::chrono::minutes kIssuesRateUpdateInterval(1);
static const qint64 kBytesInGb = 1000000000;

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

static Value ioRate(const Resource& resource, std::atomic<qint64> StorageResource::Metrics::* metric)
{
    const auto bytes = resource->getAndResetMetric(metric);
    return api::metrics::Value(double(bytes) / double(kIoRateUpdateInterval.count()));
}

static auto transactionsPerSecond(const Resource& storage)
{
    const auto ownMediaServer = storage->commonModule()->resourcePool()->getOwnMediaServerOrThrow();
    if (ownMediaServer->metadataStorageId() != storage->getId())
        return Value();
    auto statistics = storage->serverModule()->analyticsEventsStorage()->statistics();
    if (!statistics)
        return Value();
    const auto durationS = (double) duration_cast<seconds>(statistics->statisticalPeriod).count();
    return Value(statistics->requestsSucceeded / durationS);
}

static auto infoGroupProvider()
{
    return
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeSystemValueProvider<Resource>(
                "server",
                [](const auto& r) { return Value(r->getParentServer()->getName()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "type",
                [](const auto& r) { return Value(r->getStorageType()); }
            )
        );
}

static auto stateGroupProvider()
{
    return
        utils::metrics::makeValueGroupProvider<Resource>(
            "state",
            utils::metrics::makeSystemValueProvider<Resource>(
                "status",
                [](const auto& r)
                {
                    auto status = r->getStatus();
                    if (status == Qn::Online && r->isUsedForWriting())
                        status = Qn::Recording;
                    return Value(QnLexical::serialized(status));
                },
                qtSignalWatch<Resource>(
                    &QnStorageResource::statusChanged,
                    &QnStorageResource::isUsedForWritingChanged
                )
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "issues",
                [](const auto& r)
                {
                    return Value(r->getAndResetMetric(&StorageResource::Metrics::issues));
                },
                timerWatch<QnStorageResource*>(kIssuesRateUpdateInterval)
            )
        );
}

static auto activityGroupProvider()
{
    return
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            "activity",
            utils::metrics::makeLocalValueProvider<Resource>(
                "readRateBps",
                [](const auto& r) { return ioRate(r, &StorageResource::Metrics::bytesRead); },
                timerWatch<QnStorageResource*>(kIoRateUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "writeRateBps",
                [](const auto& r) { return ioRate(r, &StorageResource::Metrics::bytesWritten); },
                timerWatch<QnStorageResource*>(kIoRateUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "transactionsPerSecond",
                [](const auto& r) { return transactionsPerSecond(r); }
            )
        );
}

static auto spaceGroupProvider()
{
    return
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            "space",
            utils::metrics::makeLocalValueProvider<Resource>(
                "totalSpaceB",
                [](const auto& r) { return r->getTotalSpace(); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "mediaSpaceP",
                [](const auto& r) { return r->nxOccupedSpace() / (double) r->getTotalSpace(); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "mediaSpaceB",
                [](const auto& r) { return r->nxOccupedSpace(); }
            )
        );
}

utils::metrics::ValueGroupProviders<StorageController::Resource> StorageController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "_",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->getPath()); }
            )
        ),
        infoGroupProvider(),
        stateGroupProvider(),
        activityGroupProvider(),
        spaceGroupProvider()
    );

}

} // namespace nx::vms::server::metrics
