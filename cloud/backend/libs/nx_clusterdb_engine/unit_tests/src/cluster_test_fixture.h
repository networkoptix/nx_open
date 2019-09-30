#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <nx/clusterdb/engine/outgoing_command_filter.h>
#include <nx/clusterdb/engine/service/service.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include "customer_db/node.h"

namespace nx::clusterdb::engine::test {

class Peer
{
public:
    Peer();

    nx::utils::test::ModuleLauncher<CustomerDbNode>& process();
    const nx::utils::test::ModuleLauncher<CustomerDbNode>& process() const;

    std::string nodeId() const;

    nx::utils::Url baseApiUrl() const;

    void connectTo(const Peer& other);
    bool isConnectedTo(const Peer& other) const;
    void disconnectFrom(const Peer& other);

    Customer addRandomData();

    void addRandomDataAsync(
        nx::utils::MoveOnlyFunc<void(ResultCode, Customer)> completionHandler);

    bool hasData(const std::vector<Customer>& data);

    Customer modifyRandomly(const Customer& data);

    void setOutgoingCommandFilter(const OutgoingCommandFilterConfiguration& filter);

private:
    const std::string m_nodeId;
    nx::utils::test::ModuleLauncher<CustomerDbNode> m_process;
};

//-------------------------------------------------------------------------------------------------

class ClusterTestFixture:
    public nx::utils::test::TestWithTemporaryDirectory
{
    using base_type = nx::utils::test::TestWithTemporaryDirectory;

public:
    ClusterTestFixture();

    /**
     * Propagated to every peer to select connector type to use.
     */
    void setConnectorTypeKey(const std::string& key);

    std::string clusterId() const;

    Peer& addPeer(std::vector<std::pair<const char*, const char*>> args = {});
    Peer& peer(int index);
    const Peer& peer(int index) const;
    int peerCount() const;

    bool allPeersAreSynchronized() const;
    bool peersAreSynchronized(std::vector<int> ids) const;

private:
    std::string m_connectorTypeKey;
    const std::string m_clusterId;
    std::vector<std::unique_ptr<Peer>> m_peers;
    std::atomic<int> m_peerCounter{0};
};

} // namespace nx::clusterdb::engine::test
