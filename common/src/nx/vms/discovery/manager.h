#pragma once

#include <network/multicast_module_finder.h>

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
    }

signals:
    void found(ModuleData module); //< New reachable module is found.
    void changed(ModuleData module); //< Module information or active endpoint is changed.
    void lost(QnUuid information); //< Module connection is lost.
    void conflict(ModuleData module); //< Found module with the same UUID as we are.

private:
    void initializeConnector();
    void initializeMulticastFinders(bool clientMode);
    void monitorServerUrls(QnResourcePool* resourcePool);

private:
    std::atomic<bool> isRunning;

    mutable QnMutex m_mutex;
    std::map<QnUuid, ModuleData> m_modules;

    std::unique_ptr<ModuleConnector> m_moduleConnector;
    std::unique_ptr<UdpMulticastFinder> m_multicastFinder;
    QnMulticastModuleFinder* m_legacyMulticastFinder;
};

} // namespace discovery
} // namespace vms
} // namespace nx
