#include "network_controller.h"

#include <QJsonArray>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <nx/utils/mac_address.h>
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

    // NOTE: NIC in windows may have two different names... for Linux it is the same.
    QString humanReadableName() const
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        return m_interface.humanReadableName();
    }

    nx::utils::MacAddress hardwareAddress() const
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        const auto macStr = m_interface.hardwareAddress();
        const auto result = nx::utils::MacAddress(macStr);
        if (result.isNull())
            NX_DEBUG(this, "Failed to parse the MAC address: [%1]", macStr);
        return result;
    }

    void updateInterface(QNetworkInterface iface)
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        m_interface = std::move(iface);
    }

    nx::vms::api::metrics::Value otherAddressesJson() const
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        QJsonArray result;

        const auto addresses = m_interface.addressEntries();
        const auto displayIp = displayAddressFromAddresses(addresses);

        for (const auto& address: addresses)
        {
            if (address.ip() != displayIp)
                result.append(address.ip().toString());
        }

        if (result.empty())
            return {};
        return result;
    }

    nx::vms::api::metrics::Value displayAddress() const
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        const auto address = displayAddressFromAddresses(m_interface.addressEntries());

        if (address.isNull())
            return {};
        return address.toString();
    }

    bool isUp() const
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        return m_interface.flags().testFlag(QNetworkInterface::IsUp);
    }

    enum class Rate { in, out };
    api::metrics::Value load(Rate rate) const
    {
        if (!isUp())
            return {};

        // NOTE: Using MAC because windows has some mess with interfaces names (VMS-16182).
        const auto mac = hardwareAddress();
        const auto load = NX_METRICS_EXPECTED_ERROR(
            serverModule()->platform()->monitor()->networkInterfaceLoadOrThrow(mac),
            std::invalid_argument, "Error getting NIC load");
        return api::metrics::Value(nx::utils::switch_(rate,
            Rate::in, [&]{ return load.bytesPerSecIn; },
            Rate::out, [&]{ return load.bytesPerSecOut; }));
    }

    QnCommonModule* commonModule() const { return serverModule()->commonModule(); }

private:
    static QHostAddress displayAddressFromAddresses(const QList<QNetworkAddressEntry>& addresses)
    {
        if (addresses.empty())
            return {};

        // Trying to return IPv4 address if there is one.
        for (const auto& address: addresses)
        {
            if (address.ip().protocol() == QAbstractSocket::IPv4Protocol)
                return address.ip();
        }

        return addresses.first().ip();
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
}

utils::metrics::ValueGroupProviders<NetworkController::Resource> NetworkController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "_",
            utils::metrics::makeLocalValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->humanReadableName()); }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeLocalValueProvider<Resource>(
                "server", [this](const auto&) { return m_serverId; }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "state", [](const auto& r) { return r->isUp() ? "Up" : "Down"; }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "displayAddress", [](const auto& r) { return r->displayAddress(); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "otherAddresses", [](const auto& r) { return r->otherAddressesJson(); }
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
