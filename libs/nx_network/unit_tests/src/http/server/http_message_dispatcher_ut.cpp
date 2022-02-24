// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_basic_message_dispatcher_ut.h"

namespace nx::network::http::server::test {

INSTANTIATE_TYPED_TEST_SUITE_P(
    HttpServerMessageDispatcher,
    HttpServerBasicMessageDispatcher,
    nx::network::http::MessageDispatcher);

} // namespace nx::network::http::server::test
