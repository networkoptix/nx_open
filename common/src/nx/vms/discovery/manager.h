#pragma once

#include "deprecated_multicast_finder.h"
#include "module_connector.h"
#include "udp_multicast_finder.h"

class QnMediaServerResource;

namespace nx {
namespace vms {
namespace discovery {

struct ModuleEndpoint: QnModuleInformation
{
    SocketAddress endpoint;

    ModuleEndpoint(QnModuleInformation old = {}, SocketAddress endpoint = {});
    bool operator==(const ModuleEndpoint& rhs) const;
};

/**
 * Discovers VMS modules, notifies their module information and availability changes.
 *
 * - Searches server for endpoints by multicasts, resource pool and interface methods.
 * - Tries to maintain connection to each module and access up-to-date module information.
 * - Notifies about availability or module information changes.
 */
class Manager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    Manager(bool clientMode, QObject* parent = nullptr);
    virtual ~Manager() override;

    void setReconnectPolicy(network::RetryPolicy value);
    void setUpdateInterfacesInterval(std::chrono::milliseconds value);
    void setMulticastInterval(std::chrono::milliseconds value);

    void start();
    void stop();

    std::list<ModuleEndpoint> getAll() const; //< All accessible modules.
    boost::optional<SocketAddress> getEndpoint(const QnUuid& id) const; //< Reachable endpoint.
    boost::optional<ModuleEndpoint> getModule(const QnUuid& id) const;

    /**
     * Try to find module on the specified endpoint.
     *
     * @param endpoint Socket address to search for module information on.
     * @param expectedId If the specified address is added to module's address list and the module
     *     is already found on a different address, then this address will not be used until the
     *     address in use is disconnected. If expectedId is not scpecified, the endpoint will be
     *     pinged just once.
     */
    void checkEndpoint(SocketAddress endpoint, QnUuid expectedId = QnUuid());
    void checkEndpoint(const utils::Url &url, QnUuid expectedId = QnUuid());

    template<typename Ptr, typename Found, typename Changed, typename Lost>
    void onSignals(Ptr ptr, Found foundSlot, Changed changedSlot, Lost lostSlot)
    {
        connect(this, &Manager::found, ptr, foundSlot);
        connect(this, &Manager::changed, ptr, changedSlot);
        connect(this, &Manager::lost, ptr, lostSlot);

        for (const auto& module: getAll())
            (ptr->*foundSlot)(module);
    }

signals:
    /** New reachable module is found. */
    void found(nx::vms::discovery::ModuleEndpoint module);

    /** Module information or active endpoint of reachable module is changed. */
    void changed(nx::vms::discovery::ModuleEndpoint module);

    /** Module connection is lost and could not be restored. */
    void lost(QnUuid information);

    /** Found the module with the same UUID as we are. */
    void conflict(nx::vms::discovery::ModuleEndpoint module);

private:
    void initializeConnector();
    void initializeMulticastFinders(bool clientMode);
    void monitorServerUrls();
    void updateEndpoints(const QnMediaServerResource* server);

private:
    std::atomic<bool> isRunning;

    mutable QnMutex m_mutex;
    std::map<QnUuid, ModuleEndpoint> m_modules;

    std::unique_ptr<ModuleConnector> m_moduleConnector;
    std::unique_ptr<UdpMulticastFinder> m_multicastFinder;
    DeprecatedMulticastFinder* m_legacyMulticastFinder;
};

} // namespace discovery
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::discovery::ModuleEndpoint);
