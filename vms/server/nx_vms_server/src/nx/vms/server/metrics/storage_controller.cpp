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
            if (const auto storage = resource.dynamicCast<StorageResource>().get())
            {
                const auto addOrUpdate = [this, storage]() { add(storage, moduleGUID()); };
                QObject::connect(storage, &QnResource::parentIdChanged, addOrUpdate);
                addOrUpdate();
            }
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (const auto storage = resource.dynamicCast<QnStorageResource>())
                remove(storage->getId());
        });
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
                [](const auto& r) { return Value(r->getParentId().toSimpleString()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "pool",
                [](const auto& r)
                {
                    if (!r->isUsedForWriting())
                        return "Reserved";
                    return r->isBackup() ? "Backup" : "Main";
                }
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
                "systemStatus",
                [](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
                qtSignalWatch<Resource>(&QnStorageResource::statusChanged)
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "status",
                [](const auto& r)
                {
                    if (isServerOffline(r))
                        return Value("Server Offline");
                    auto status = r->getStatus();
                    if (status == Qn::Online && r->isUsedForWriting())
                        status = Qn::Recording;
                    return Value(QnLexical::serialized(status));
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "issues",
                [](const auto& r) { return Value(r->getMetric(&StorageResource::Metrics::issues)); },
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
                [](const auto& r) { return r->getMetric(&StorageResource::Metrics::bytesRead); },
                timerWatch<QnStorageResource*>(kIoRateUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "writeRateBps",
                [](const auto& r) { return r->getMetric(&StorageResource::Metrics::bytesWritten); },
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
                [](const auto& r)
                {
                    if (const auto space = r->getTotalSpace(); space > 0)
                        return Value(space);
                    return Value();
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "mediaSpaceB",
                [](const auto& r)
                {
                    if (const auto space = r->nxOccupedSpace(); space > 0)
                        return Value(space);
                    return Value();
                }
            )
        );
}

utils::metrics::ValueGroupProviders<StorageController::Resource> StorageController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "_",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name",
                [](const auto& r) -> Value
                {
                    const auto urlString = r->getUrl();
                    if (!urlString.contains("://"))
                        return urlString;

                    const nx::utils::Url url(urlString);
                    return url.host() + url.path();
                }
            )
        ),
        infoGroupProvider(),
        stateGroupProvider(),
        activityGroupProvider(),
        spaceGroupProvider()
    );

}

} // namespace nx::vms::server::metrics
