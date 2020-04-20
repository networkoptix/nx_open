#pragma once

#include <nx/network/http/http_async_client.h>

namespace nx::vms_server_plugins::analytics::vivotek {

void checkResponse(const nx::network::http::AsyncClient& client);

} // namespace nx::vms_server_plugins::analytics::vivotek
