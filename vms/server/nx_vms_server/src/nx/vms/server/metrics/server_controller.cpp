#include "server_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <platform/hardware_information.h>
#include <recorder/storage_manager.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

ServerController::ServerController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<QnMediaServerResource*>("servers", makeProviders())
{
}

void ServerController::start()
{
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                add(std::make_unique<ResourceDescription<Resource>>(server.get()));
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                remove(server->getId().toSimpleString());
        });
}

bool ServerController::isLocal(const Resource& resource) const
{
    return resource->getId() == moduleGUID();
}

utils::metrics::Getter<ServerController::Resource> ServerController::localGetter(
    utils::metrics::Getter<Resource> getter) const
{
    return
        [this, getter = std::move(getter), isLocalCache = std::unique_ptr<bool>()](
            const Resource& resource) mutable
        {
            if (!isLocalCache)
                isLocalCache = std::make_unique<bool>(isLocal(resource));

            return *isLocalCache ? getter(resource) : Value();
        };
}

utils::metrics::ValueGroupProviders<ServerController::Resource> ServerController::makeProviders()
{
    using namespace std::chrono;

    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            api::metrics::Label(
                "state"
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest({"name"}, api::metrics::Display::both),
                [](const auto& r) { return Value(r->getName()); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest({"status"}, api::metrics::Display::both),
                [](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
                qtSignalWatch<Resource>(&QnStorageResource::statusChanged)
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest({"uptime"}, api::metrics::Display::both, "s"),
                localGetter(
                    [start = steady_clock::now()](const auto&)
                    {
                        return Value((double) duration_cast<seconds>(
                            steady_clock::now() - start).count());
                    }
                )
            )
        )
        // TODO: Implement "Server load", "Info" and "Activity" groups.
    );
}

} // namespace nx::vms::server::metrics

