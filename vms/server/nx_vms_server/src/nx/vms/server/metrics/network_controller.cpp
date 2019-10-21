#include "network_controller.h"

#include <media_server/media_server_module.h>
#include "helpers.h"

namespace nx::vms::server::metrics {

namespace {

// TODO: change to 1 minute
std::chrono::seconds interfacesPollingPeriod(10);

std::set<QNetworkInterface, NetworkController::InterfacesCompare> interfacesSetDifference(
    const std::set<QNetworkInterface, NetworkController::InterfacesCompare>& left,
    const std::set<QNetworkInterface, NetworkController::InterfacesCompare>& right)
{
    std::set<QNetworkInterface, NetworkController::InterfacesCompare> result;
    std::set_difference(
        left.begin(), left.end(),
        right.begin(), right.end(),
        std::inserter(result, result.end()),
        NetworkController::InterfacesCompare());
    return result;
}

} // namespace

NetworkController::NetworkController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<QNetworkInterface>(
        "networkInterfaces", makeProviders()),
    m_serverId(serverModule->commonModule()->moduleGUID().toSimpleString())
{
}

void NetworkController::start()
{
    updateInterfaces();
    m_timerGuard = makeTimer(
        serverModule()->timerManager(),
        interfacesPollingPeriod,
        [this]() { updateInterfaces(); });
}

utils::metrics::ValueGroupProviders<NetworkController::Resource> NetworkController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "_",
            utils::metrics::makeLocalValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r.name()); }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeLocalValueProvider<Resource>(
                "server", [this](const auto&) { return Value(m_serverId); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ip", [](const auto&) { return Value(); }
            ) // TODO: implement ip
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "rates",
            utils::metrics::makeLocalValueProvider<Resource>(
                "inBps", [](const auto&) { return Value(0); } // TODO: Implement.
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "outBps", [](const auto&) { return Value(0); } // TODO: Implement.
            )
        )
    );

    // TODO: add multiple ip list
    // TODO: add state
}

QString NetworkController::interfaceIdFromName(const QString& name) const
{
    return m_serverId + "_" + name;
}

void NetworkController::updateInterfaces()
{
    std::set<QNetworkInterface, InterfacesCompare> newInterfaces;
    for (const auto& iface: QNetworkInterface::allInterfaces())
    {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        newInterfaces.insert(iface);
    }

    const auto discoveredInterfaces = interfacesSetDifference(newInterfaces, m_currentInterfaces);
    for (const auto& iface: discoveredInterfaces)
        add(std::move(iface), interfaceIdFromName(iface.name()), utils::metrics::Scope::local);

    const auto disappearedInterfaces = interfacesSetDifference(m_currentInterfaces, newInterfaces);
    for (const auto& iface: disappearedInterfaces)
        remove(interfaceIdFromName(iface.name()));

    m_currentInterfaces = std::move(newInterfaces);
}

} // namespace nx::vms::server::metrics



