#pragma once

#include <atomic>
#include <memory>

#include <gtest/gtest.h>

#include <nx/data_sync_engine/service/service.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include "customer_db/node.h"

namespace nx::data_sync_engine::test {

class Peer
{
public:
    nx::utils::test::ModuleLauncher<CustomerDbNode>& process();
    const nx::utils::test::ModuleLauncher<CustomerDbNode>& process() const;

    void connectTo(const Peer& other);
    void addRandomData();

private:
    nx::utils::test::ModuleLauncher<CustomerDbNode> m_process;
};

//-------------------------------------------------------------------------------------------------

class ClusterTestFixture:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
    using base_type = nx::utils::test::TestWithTemporaryDirectory;

public:
    ClusterTestFixture();

    void addPeer();
    Peer& peer(int index);
    const Peer& peer(int index) const;
    int peerCount() const;

    bool allPeersAreSynchronized() const;

private:
    std::vector<std::unique_ptr<Peer>> m_peers;
    std::atomic<int> m_peerCounter{0};
};

} // namespace nx::data_sync_engine::test
