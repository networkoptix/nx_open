#pragma once

#include "module_connector.h"
#include "udp_multicast_finder.h"
#include <network/multicast_module_finder.h>

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
    Manager(QnCommonModule* commonModule, bool clientMode);
    ~Manager();

    struct ModuleData: QnModuleInformation
    {
        SocketAddress endpoint;

        ModuleData();
        ModuleData(QnModuleInformation old, SocketAddress endpoint);
    };

    std::list<ModuleData> getAll() const; //< All accessible modules.
    boost::optional<SocketAddress> getEndpoint(const QnUuid& id) const; //< Reachable endpoint.
    void checkUrl(const SocketAddress& endpoint); //< Try for find module on address.

signals:
    void found(ModuleData module); //< New reachable module is found.
    void changed(ModuleData module); //< Module information or active endpoint is changed.
    void lost(QnUuid information); //< Module connection is lost.
    void conflict(ModuleData module); //< Found module with the same UUID as we are.

private:
    void initializeConnector();
    void initializeMulticastFinders(bool clientMode);

private:
    mutable QnMutex m_mutex;
    std::map<QnUuid, ModuleData> m_modules;

    std::unique_ptr<ModuleConnector> m_moduleConnector;
    std::unique_ptr<UdpMulticastFinder> m_multicastFinder;
    QnMulticastModuleFinder* m_legacyMulticastFinder;
};

} // namespace discovery
} // namespace vms
} // namespace nx
