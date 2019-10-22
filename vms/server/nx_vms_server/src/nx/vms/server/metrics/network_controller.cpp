#include "network_controller.h"

#include "helpers.h"

#include <media_server/media_server_module.h>
#include <QJsonArray>

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

QJsonArray interfacesAddresses(QNetworkInterface interface)
{
    QJsonArray result;
    for (const QNetworkAddressEntry& address: interface.addressEntries())
        result.append(address.ip().toString());
    return result;
}

QString firstAddress(QNetworkInterface interface)
{
    const auto addresses = interface.addressEntries();
    if (addresses.size() > 0)
        return addresses.first().ip().toString();
    return {};
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

// TODO: What about local interface wrapper with mutexes inside to avoid boilerplate?
//       Or make resource just a simple string...
//       Or implement something similar to resource (just sharedPtr to the QNetworkInterface?)
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
                "state",
                [this](const auto& r)
                {
                    NX_MUTEX_LOCKER locker(&m_mutex);
                    auto iface = m_currentInterfaces.find(r);
                    if (iface == m_currentInterfaces.end())
                        return Value();
                    return Value(iface->flags().testFlag(QNetworkInterface::IsUp) ? "Up" : "Down");
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ipList",
                [this](const auto& r)
                {
                    NX_MUTEX_LOCKER locker(&m_mutex);
                    auto iface = m_currentInterfaces.find(r);
                    if (iface == m_currentInterfaces.end())
                        return Value();
                    return Value(interfacesAddresses(*iface));
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "firstIp",
                [this](const auto& r)
                {
                    NX_MUTEX_LOCKER locker(&m_mutex);
                    auto iface = m_currentInterfaces.find(r);
                    if (iface == m_currentInterfaces.end())
                        return Value();
                    return Value(firstAddress(*iface));
                }
            )
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

    std::set<QNetworkInterface, InterfacesCompare> previousInterfaces;
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        previousInterfaces = std::move(m_currentInterfaces);
        m_currentInterfaces = newInterfaces;
    }

    for (const auto& iface: interfacesSetDifference(newInterfaces, previousInterfaces))
        add(std::move(iface), interfaceIdFromName(iface.name()), utils::metrics::Scope::local);

    for (const auto& iface: interfacesSetDifference(previousInterfaces, newInterfaces))
        remove(interfaceIdFromName(iface.name()));
}

} // namespace nx::vms::server::metrics



