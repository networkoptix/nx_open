#include "relay_test_setup.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

RelayTestSetup::RelayTestSetup():
    utils::test::TestWithTemporaryDirectory("traffic_relay", QString())
{
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
