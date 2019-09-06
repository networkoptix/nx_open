#pragma once

#include "resource_tree_model_test_fixture.h"
#include "resource_tree_model_index_condition.h"

namespace nx::vms::client::desktop {
namespace test {

using ConditionWithCaption = std::tuple<index_condition::Condition, std::string>;

class ResourceTreeModelSortingTest: public ResourceTreeModelTest,
    public testing::WithParamInterface<ConditionWithCaption>
{
public:
    struct PrintToStringParamName
    {
        template <class ParamType>
        std::string operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            return std::get<1>(info.param);
        };
    };
};

} // namespace test
} // namespace nx::vms::client::desktop
