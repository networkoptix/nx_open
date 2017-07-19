#pragma once

#include "http_server_rest_path_matcher.h"
#include "../http_message_dispatcher.h"

namespace nx_http {
namespace server {
namespace rest {

class MessageDispatcher:
    public BasicMessageDispatcher<PathMatcher>
{
};

} // namespace rest
} // namespace server
} // namespace nx_http
