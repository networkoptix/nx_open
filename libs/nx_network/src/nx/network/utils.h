// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "http/abstract_msg_body_source.h"
#include "http/server/request_processing_types.h"

namespace nx::network::utils {

std::optional<std::string> NX_NETWORK_API getHostName(
    const http::RequestContext& request, const std::string& hostHeader = "Host");

} // namespace nx::network::utils
