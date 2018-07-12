#pragma once

namespace nx {
namespace cloud {
namespace relay {
namespace test {

relaying::Statistics generateRelayingStatistics();
controller::RelaySessionStatistics generateRelaySessionStatistics();
nx::network::server::Statistics generateHttpServerStatistics();

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
