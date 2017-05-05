#include "basic_component_test.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

BasicComponentTest::BasicComponentTest(QString tmpDir):
    utils::test::TestWithTemporaryDirectory("traffic_relay", tmpDir)
{
}

BasicComponentTest::~BasicComponentTest()
{
    stop();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
