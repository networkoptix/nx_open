#pragma once

#include "deprecated_multicast_finder.h"
#include "module_connector.h"
#include "udp_multicast_finder.h"

namespace nx {
namespace vms {
namespace discovery {

/**
 *
 */
class Manager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
    Manager(QnCommonModule* commonModule, bool clientMode, QnResourcePool* resourcePool);
    virtual ~Manager() override;

    void start();
    void stop();

    struct ModuleData: QnModuleInformation
    {
        SocketAddress endpoint;

        ModuleData();
        ModuleData(QnModuleInformation old, SocketAddress endpoint);
    };

    std::list<ModuleData> getAll() const; //< All accessible modules.
    boost::optional<SocketAddress> getEndpoint(const QnUuid& id) const; //< Reachable endpoint.
    boost::optional<ModuleData> getModule(const QnUuid& id) const;

    void checkEndpoint(const SocketAddress& endpoint); //< Try for find module on address.
    void checkEndpoint(const QUrl& url);

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
    void found(nx::vms::discovery::Manager::ModuleData module);

    /** Module information or active endpoint of reachable module is changed. */
    void changed(nx::vms::discovery::Manager::ModuleData module);

    /** Module connection is lost and could not be restored. */
    void lost(QnUuid information);

    /** Found the module with the same UUID as we are. */
    void conflict(nx::vms::discovery::Manager::ModuleData module);

private:
    void initializeConnector();
    void initializeMulticastFinders(bool clientMode);
    void monitorServerUrls();

private:
    QnResourcePool* const m_resourcePool;
    std::atomic<bool> isRunning;

    mutable QnMutex m_mutex;
    std::map<QnUuid, ModuleData> m_modules;

    std::unique_ptr<ModuleConnector> m_moduleConnector;
    std::unique_ptr<UdpMulticastFinder> m_multicastFinder;
    DeprecatedMulticastFinder* m_legacyMulticastFinder;
};

} // namespace discovery
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::discovery::Manager::ModuleData);
