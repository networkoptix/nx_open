#include "network_controller.h"

#include "helpers.h"

#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>
#include <QJsonArray>

namespace nx::vms::server::metrics {

namespace {

// TODO: change to 1 minute
std::chrono::seconds interfacesPollingPeriod(10);

std::set<QString> stringSetDifference(
    const std::set<QString>& left,
    const std::set<QString>& right)
{
    std::set<QString> result;
    std::set_difference(
        left.begin(), left.end(),
        right.begin(), right.end(),
        std::inserter(result, result.end()));
    return result;
}

} // namespace

class InterfaceResource
{
public:
    InterfaceResource(QNetworkInterface iface)
        : m_interface(std::move(iface))
    {}

    QString name() const
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        return m_interface.name();
    }

    void updateInterface(QNetworkInterface iface)
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        m_interface = std::move(iface);
    }

    QJsonArray addressesJson() const
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        QJsonArray result;
        for (const auto& address: m_interface.addressEntries())
            result.append(address.ip().toString());
        return result;
    }

    QString firstAddress() const
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        const auto addresses = m_interface.addressEntries();
        if (addresses.size() > 0)
            return addresses.first().ip().toString();
        return {};
    }

    QString state() const
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        return m_interface.flags().testFlag(QNetworkInterface::IsUp) ? "Up" : "Down";
    }

private:
    QNetworkInterface m_interface;
    mutable nx::utils::Mutex m_mutex;
};

NetworkController::NetworkController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<std::shared_ptr<InterfaceResource>>(
        "networkInterfaces", makeProviders()),
    m_serverId(serverModule->commonModule()->moduleGUID().toSimpleString())
{
}

void NetworkController::start()
{
    updateInterfacesPool();
    m_timerGuard = makeTimer(
        serverModule()->timerManager(),
        interfacesPollingPeriod,
        [this]() { updateInterfacesPool(); });
}

utils::metrics::ValueGroupProviders<NetworkController::Resource> NetworkController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "_",
            utils::metrics::makeLocalValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->name()); }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeLocalValueProvider<Resource>(
                "server",
                [this](const auto& r)
                {
                    const auto server = resourcePool()->getResourceById(QnUuid(m_serverId));
                    return Value(server->getName());
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "state", [this](const auto& r) { return r->state(); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ipList", [this](const auto& r) { return r->addressesJson(); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "firstIp", [this](const auto& r) { return r->firstAddress(); }
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

void NetworkController::updateInterfacesPool()
{
    std::set<QString> previousIfacesByName;
    for (const auto& ifaceEntry: m_interfacesPool)
        previousIfacesByName.insert(ifaceEntry.first);

    const auto newInterfacesList = QNetworkInterface::allInterfaces();
    std::set<QString> newIfacesByName;
    for (const auto& iface: newInterfacesList)
    {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        newIfacesByName.insert(iface.name());
    }

    const auto discoveredInterfaces = stringSetDifference(newIfacesByName, previousIfacesByName);
    const auto disappearedInterfaces = stringSetDifference(previousIfacesByName, newIfacesByName);

    for (const auto& iface: disappearedInterfaces)
        m_interfacesPool.erase(iface);
    for (const auto& iface: newInterfacesList)
    {
        if (discoveredInterfaces.find(iface.name()) != discoveredInterfaces.end())
            m_interfacesPool[iface.name()] = std::make_shared<InterfaceResource>(iface);
        else if (m_interfacesPool.find(iface.name()) != m_interfacesPool.end())
            m_interfacesPool[iface.name()]->updateInterface(iface);
    }

    for (const auto& iface: discoveredInterfaces)
        add(m_interfacesPool[iface], interfaceIdFromName(iface), utils::metrics::Scope::local);
    for (const auto& iface: disappearedInterfaces)
        remove(interfaceIdFromName(iface));
}

} // namespace nx::vms::server::metrics



