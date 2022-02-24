// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "http/abstract_msg_body_source.h"
#include "http/server/request_processing_types.h"

namespace nx::network::util {

std::optional<std::string> NX_NETWORK_API getHostName(const http::RequestContext& request);

} // namespace nx::network::util
