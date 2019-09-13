#include "server_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <platform/hardware_information.h>
#include <recorder/storage_manager.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

namespace {

class ServerDescription: public utils::metrics::ResourceDescription<QnMediaServerResource*>
{
public:
    ServerDescription(QnMediaServerResource* resource, QnUuid ownId):
        utils::metrics::ResourceDescription<QnMediaServerResource*>(resource),
        m_ownId(std::move(ownId))
    {
    }

    QString id() const override
    {
        return this->resource->getId().toSimpleString();
    }

    utils::metrics::Scope scope() const override
    {
        return this->resource->getId() == m_ownId
            ? utils::metrics::Scope::local
            : utils::metrics::Scope::system;
    }

private:
    QnUuid m_ownId;
};

} // namespace

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
                add(std::make_unique<ServerDescription>(server.get(), moduleGUID()));
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                remove(server->getId().toSimpleString());
        });
}

utils::metrics::ValueGroupProviders<ServerController::Resource> ServerController::makeProviders()
{
    const auto getUptimeS =
        [start = std::chrono::steady_clock::now()](const auto&)
        {
            return Value((double) std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count());
        };

    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "state",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->getName()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "status",
                [](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
                qtSignalWatch<Resource>(&QnResource::statusChanged)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "uptimeS", getUptimeS
            )
        )
        // TODO: Implement "Server load", "Info" and "Activity" groups.
    );
}

} // namespace nx::vms::server::metrics

