#include <gtest/gtest.h>

#include <nx/vms/api/data/tran_state_data.h>

namespace nx::clusterdb::engine {

class TranState:
    public ::testing::Test
{
protected:
    struct TranStateData
    {
        std::string id;
        int sequence = 0;
    };

    nx::vms::api::TranState build(
        std::vector<TranStateData> data)
    {
        nx::vms::api::TranState result;

        for (const auto& val: data)
        {
            nx::vms::api::PersistentIdData id;
            id.id = QnUuid::fromArbitraryData(val.id);
            id.persistentId = QnUuid::fromArbitraryData(val.id);
            result.values.insert(id, val.sequence);
        }

        return result;
    }

    bool containsDataMissingIn(
        const nx::vms::api::TranState& one,
        const nx::vms::api::TranState& two) const
    {
        return one.containsDataMissingIn(two);
    }
};

TEST_F(TranState, containsDataMissingIn)
{
    ASSERT_TRUE (containsDataMissingIn(build({{"A", 1}, {"B", 22}}), build({{"B", 22}})));
    ASSERT_FALSE(containsDataMissingIn(build({{"B", 22}}),           build({{"B", 22}})));
    ASSERT_FALSE(containsDataMissingIn(build({{"B", 22}}),           build({{"A", 1}, {"B", 22}})));
    ASSERT_FALSE(containsDataMissingIn(build({}),                    build({{"B", 22}})));
    ASSERT_TRUE (containsDataMissingIn(build({{"A", 1}, {"B", 22}}), build({})));
}

} // namespace nx::clusterdb::engine
