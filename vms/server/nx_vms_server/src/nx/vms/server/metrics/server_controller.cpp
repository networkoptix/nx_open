#include "server_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <platform/hardware_information.h>
#include <platform/platform_abstraction.h>
#include <nx/metrics/metrics_storage.h>

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
            {
                add(server.get(), server->getId(), (server->getId() == moduleGUID())
                    ? utils::metrics::Scope::local
                    : utils::metrics::Scope::system);
            }
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                remove(server->getId());
        });
}

utils::metrics::ValueGroupProviders<ServerController::Resource> ServerController::makeProviders()
{
    using namespace ResourcePropertyKey;

    // TODO: make sure that platform is removed only after HM!
    auto platform = serverModule()->platform()->monitor();
    const auto getUptimeS =
        [platform](const auto&)
        {
            return Value(static_cast<qint64>(platform->processUptime().count()) / 1000);
        };

    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->getName()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "cpu", [](const auto& r) { return Value(r->getProperty(Server::kCpuModelName)); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "ram", [](const auto& r) { return Value(r->getProperty(Server::kPhysicalMemory)); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "os", [](const auto& r) { return Value(r->getProperty(Server::kSystemRuntime)); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "publicIp", [](const auto& r) { return Value(r->getProperty(Server::kPublicIp)); }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "availability",
            utils::metrics::makeSystemValueProvider<Resource>(
                "status",
                [](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
                qtSignalWatch<Resource>(&QnResource::statusChanged)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "uptimeS", getUptimeS
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "serverLoad",
            utils::metrics::makeLocalValueProvider<Resource>(
                "cpuP", [platform](const auto&) { return Value(platform->totalCpuUsage()); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ramP", [platform](const auto&) { return Value(platform->totalRamUsage()); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "incomingConnections", [](const auto& r)
                    { return Value(r->commonModule()->metrics()->tcpConnections().total()); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "encodingThreads", [](const auto& r)
                    { return Value(r->commonModule()->metrics()->transcoders()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "cameras", [](const auto& r)
                    {
                        return Value(r->resourcePool()->getAllCameras(
                            r->toSharedPointer(), /*ignoreDesktopCameras*/ true).size());
                    }
            )
      )
        // TODO: Implement "Server load", "Info" and "Activity" groups.
    );
}

} // namespace nx::vms::server::metrics
