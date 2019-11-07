#include "network_controller.h"

#include <QJsonArray>

#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/switch.h>
#include <platform/platform_abstraction.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

namespace {

constexpr std::chrono::seconds kInterfacesCacheExpiration(7);
constexpr std::chrono::seconds kIoRateUpdateInterval(5);

} // namespace

class NetworkInterfaceResource: public ServerModuleAware
{
public:
    NetworkInterfaceResource(QnMediaServerModule* serverModule, QNetworkInterface iface):
        ServerModuleAware(serverModule),
        m_interface(std::move(iface))
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

    nx::vms::api::metrics::Value addressesJson() const
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        QJsonArray result;
        for (const auto& address: m_interface.addressEntries())
            result.append(address.ip().toString());
        if (result.empty())
            return {};
        return result;
    }

    nx::vms::api::metrics::Value firstAddress() const
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

    enum class Rate { in, out };
    api::metrics::Value load(Rate rate) const
    {
        const auto load = serverModule()->platform()->monitor()->networkInterfaceLoad(name());
        if (!load)
            return api::metrics::Value();
        return api::metrics::Value(nx::utils::switch_(rate,
            Rate::in, [&]{ return load->bytesPerSecIn; },
            Rate::out, [&]{ return load->bytesPerSecOut; }));
    }

    QnCommonModule* commonModule() const { return serverModule()->commonModule(); }

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
                "inBps",
                [](const auto& r) { return r->load(NetworkInterfaceResource::Rate::in); },
                timerWatch<Resource>(kIoRateUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "outBps",
                [](const auto& r) { return r->load(NetworkInterfaceResource::Rate::out); },
                timerWatch<Resource>(kIoRateUpdateInterval)
            )
        )
    );
}

void NetworkController::beforeValues(utils::metrics::Scope, bool)
{
    updateInterfacesPool();
}

void NetworkController::beforeAlarms(utils::metrics::Scope)
{
    updateInterfacesPool();
}

QString NetworkController::interfaceIdFromName(const QString& name) const
{
    return m_serverId + "_" + name;
}

void NetworkController::updateInterfacesPool()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_lastInterfacesUpdate.hasExpired(kInterfacesCacheExpiration))
        return;

    NX_VERBOSE(this, "Updating network interfaces info");
    const auto newInterfacesList = nx::utils::copy_if(QNetworkInterface::allInterfaces(),
        [](const auto& iface){ return !iface.flags().testFlag(QNetworkInterface::IsLoopBack); });

    removeDisappearedInterfaces(newInterfacesList);
    addOrUpdateInterfaces(std::move(newInterfacesList));

    m_lastInterfacesUpdate.restart();
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
            m_interfacesPool[name] = std::make_shared<NetworkInterfaceResource>(
                serverModule(), std::move(iface));
            add(m_interfacesPool[name], interfaceIdFromName(name),
                utils::metrics::Scope::local);
            continue;
        }

        m_interfacesPool[name]->updateInterface(std::move(iface));
    }
}

} // namespace nx::vms::server::metrics
