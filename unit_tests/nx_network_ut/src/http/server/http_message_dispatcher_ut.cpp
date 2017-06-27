#include "http_server_basic_message_dispatcher_ut.h"

namespace nx_http {
namespace server {
namespace test {

INSTANTIATE_TYPED_TEST_CASE_P(
    HttpServerMessageDispatcher,
    HttpServerBasicMessageDispatcher,
    nx_http::MessageDispatcher);

} // namespace test
} // namespace server
} // namespace nx_http
