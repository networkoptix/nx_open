#include <gtest/gtest.h>

#include <nx/clusterdb/engine/node_state.h>

namespace nx::clusterdb::engine::test {

class NodeState:
    public ::testing::Test
{
protected:
    struct NodeStateData
    {
        std::string id;
        int sequence = 0;
    };

    engine::NodeState build(std::vector<NodeStateData> data)
    {
        engine::NodeState result;

        for (const auto& item: data)
        {
            const auto id = QnUuid::fromArbitraryData(item.id);
            result.nodeSequence[NodeStateKey{id, id}] = item.sequence;
        }

        return result;
    }

    bool containsDataMissingIn(
        const engine::NodeState& one,
        const engine::NodeState& two) const
    {
        return one.containsDataMissingIn(two);
    }
};

TEST_F(NodeState, containsDataMissingIn)
{
    ASSERT_TRUE (containsDataMissingIn(build({{"A", 1}, {"B", 22}}), build({{"B", 22}})));
    ASSERT_FALSE(containsDataMissingIn(build({{"B", 22}}),           build({{"B", 22}})));
    ASSERT_FALSE(containsDataMissingIn(build({{"B", 22}}),           build({{"A", 1}, {"B", 22}})));
    ASSERT_FALSE(containsDataMissingIn(build({}),                    build({{"B", 22}})));
    ASSERT_TRUE (containsDataMissingIn(build({{"A", 1}, {"B", 22}}), build({})));
}

} // namespace nx::clusterdb::engine::test
