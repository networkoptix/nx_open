#pragma once

namespace nx {
namespace cloud {
namespace relay {
namespace test {

relaying::Statistics generateRelayingStatistics();
controller::RelaySessionStatistics generateRelaySessionStatistics();
nx::network::http::server::HttpStatistics generateHttpServerStatistics();

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
