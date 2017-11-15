#include <gtest/gtest.h>

#include "../basic_component_test.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class Statistics:
    public ::testing::Test,
    public BasicComponentTest
{
private:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
