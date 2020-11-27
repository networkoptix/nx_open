#pragma once

#include <nx/network/http/http_types.h>

namespace nx::vms_server_plugins::utils {

void verifySuccessOrThrow(nx::network::http::StatusCode::Value code);
void verifySuccessOrThrow(const nx::network::http::StatusLine& line);
void verifySuccessOrThrow(const nx::network::http::Response& response);

} // namespace nx::vms_server_plugins::utils
