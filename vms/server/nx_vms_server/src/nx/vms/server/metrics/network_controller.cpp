#include "network_controller.h"

#include "helpers.h"

#include <QJsonArray>
#include <nx/utils/std/algorithm.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>

namespace nx::vms::server::metrics {

namespace {

// TODO: change to 1 minute
constexpr std::chrono::seconds kInterfacesPollingPeriod(10);

} // namespace

class NetworkInterfaceResource
{
public:
    NetworkInterfaceResource(QNetworkInterface iface)
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
        if (!addresses.empty())
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
    utils::metrics::ResourceControllerImpl<std::shared_ptr<NetworkInterfaceResource>>(
        "networkInterfaces", makeProviders()),
    m_serverId(serverModule->commonModule()->moduleGUID().toSimpleString())
{
}

void NetworkController::start()
{
    updateInterfacesPool();
    m_timerGuard = makeTimer(
        serverModule()->timerManager(),
        kInterfacesPollingPeriod,
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
                [this](const auto&)
                {
                    const auto server = resourcePool()->getResourceById(QnUuid(m_serverId));
                    return Value(server->getName());
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "state", [this](const auto& r) { return r->state(); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "firstIp", [this](const auto& r) { return r->firstAddress(); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ipList", [this](const auto& r) { return r->addressesJson(); }
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
    const auto newInterfacesList = nx::utils::filter_if(QNetworkInterface::allInterfaces(),
        [](const auto& iface){ return !iface.flags().testFlag(QNetworkInterface::IsLoopBack); });

    removeDisappearedInterfaces(newInterfacesList);
    addOrUpdateInterfaces(std::move(newInterfacesList));
}

void NetworkController::removeDisappearedInterfaces(const QList<QNetworkInterface>& newIfaces)
{
    for (auto it = m_interfacesPool.begin(); it != m_interfacesPool.end();)
    {
        if (nx::utils::find_if(newIfaces,
            [&it](const auto& iface){ return iface.name() == it->first; }) == nullptr)
        {
            remove(interfaceIdFromName(it->first));
            it = m_interfacesPool.erase(it);
            continue;
        }

        it++;
    }
}

void NetworkController::addOrUpdateInterfaces(QList<QNetworkInterface> newIfaces)
{
    for (auto& iface: newIfaces)
    {
        const auto name = iface.name();
        if (m_interfacesPool.find(name) == m_interfacesPool.end())
        {
            m_interfacesPool[name] = std::make_shared<NetworkInterfaceResource>(std::move(iface));
            add(m_interfacesPool[name], interfaceIdFromName(name),
                utils::metrics::Scope::local);
            continue;
        }

        m_interfacesPool[name]->updateInterface(std::move(iface));
    }
}

} // namespace nx::vms::server::metrics



