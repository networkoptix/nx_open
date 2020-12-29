#include "http_server_basic_message_dispatcher_ut.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace test {

INSTANTIATE_TYPED_TEST_CASE_P(
    HttpServerMessageDispatcher,
    HttpServerBasicMessageDispatcher,
    nx::network::http::MessageDispatcher);

} // namespace test
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
