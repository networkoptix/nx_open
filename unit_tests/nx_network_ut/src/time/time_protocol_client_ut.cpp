#include "basic_time_protocol_client_tests.h"

#include <nx/network/time/time_protocol_client.h>

namespace nx {
namespace network {
namespace test {

INSTANTIATE_TYPED_TEST_CASE_P(
    TimeProtocolClient,
    BasicTimeProtocolClient,
    TimeProtocolClient);

} // namespace test
} // namespace network
} // namespace nx
