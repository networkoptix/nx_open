#pragma once

#include "http_server_rest_path_matcher.h"
#include "../http_message_dispatcher.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace rest {

class MessageDispatcher:
    public BasicMessageDispatcher<PathMatcher>
{
};

} // namespace rest
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
