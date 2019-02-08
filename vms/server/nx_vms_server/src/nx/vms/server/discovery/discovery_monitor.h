#pragma once

#include <nx/vms/api/data/discovery_data.h>
#include <transaction/transaction.h>
#include <nx/vms/discovery/manager.h>

namespace nx {
namespace vms::server {
namespace discovery {

nx::vms::api::DiscoveredServerDataList getServers(nx::vms::discovery::Manager* manager);

class DiscoveryMonitor: public QObject, public QnCommonModuleAware
{
public:
    DiscoveryMonitor(ec2::TransactionMessageBusAdapter* messageBus);
    virtual ~DiscoveryMonitor();

private:
    void clientFound(QnUuid peerId, nx::vms::api::PeerType peerType);
    void serverFound(nx::vms::discovery::ModuleEndpoint module);
    void serverLost(QnUuid id);

    template<typename Transaction, typename Target>
    void send(ec2::ApiCommand::Value command, Transaction data, const Target& target);

private:
    ec2::TransactionMessageBusAdapter* m_messageBus = nullptr;
    std::map<QnUuid, nx::vms::api::DiscoveredServerData> m_serverCache;
};

} // discovery
} // mediaserver
} // nx
