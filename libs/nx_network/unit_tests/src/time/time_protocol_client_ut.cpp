// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_time_protocol_client_tests.h"

#include <nx/network/time/time_protocol_client.h>

namespace nx {
namespace network {
namespace test {

INSTANTIATE_TYPED_TEST_SUITE_P(
    TimeProtocolClient,
    BasicTimeProtocolClient,
    TimeProtocolClient);

} // namespace test
} // namespace network
} // namespace nx
