#include <gtest/gtest.h>

#include "cluster_test_fixture.h"

namespace nx::clusterdb::engine::test {

class LoadTest:
    public ::testing::Test,
    public ClusterTestFixture
{
protected:
    void givenConnectedNodes(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            addPeer(m_args);
            if (i > 0)
                peer(i).connectTo(peer(0));
        }
    }

    void addDataToOneNode()
    {
        constexpr int kItemToAddCount = 77;

        std::atomic<int> itemsAdded{0};
        for (int i = 0; i < kItemToAddCount; ++i)
        {
            peer(0).addRandomDataAsync(
                [&itemsAdded](ResultCode /*resultCode*/, Customer /*customer*/)
                {
                    ++itemsAdded;
                });
        }

        while (itemsAdded.load() < kItemToAddCount)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void waitForNodesToSynchronize()
    {
        while (!allPeersAreSynchronized())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void enableCommandGrouppingUnderDbTransaction()
    {
        m_args.push_back({"-p2pDb/groupCommandsUnderDbTransaction", "true"});
    }

private:
    std::vector<std::pair<const char*, const char*>> m_args;
};

TEST_F(LoadTest, syncronizing_nodes)
{
    enableCommandGrouppingUnderDbTransaction();

    givenConnectedNodes(2);
    addDataToOneNode();
    waitForNodesToSynchronize();
}

} // namespace nx::clusterdb::engine::test
