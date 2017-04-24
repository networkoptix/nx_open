#include "../http_server_basic_message_dispatcher_ut.h"

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx_http {
namespace server {
namespace test {

INSTANTIATE_TYPED_TEST_CASE_P(
    HttpServerRestMessageDispatcher,
    HttpServerBasicMessageDispatcher,
    nx_http::server::rest::MessageDispatcher);

} // namespace test
} // namespace server
} // namespace nx_http
