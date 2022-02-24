// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include "connection_info.h"

namespace nx::vms::client::core {

/** Return information needed for a connection to the system with the specified cloud system id. */
std::optional<ConnectionInfo> getCloudConnectionInfo(const QString& systemId);

/**
 *  Return information needed for a connection to the local system with specified url and
 *  credentials.
 */
std::optional<ConnectionInfo> getLocalConnectionInfo(
    const nx::utils::Url& url,
    const nx::network::http::Credentials& credentials);

} // namespace nx::vms::client::core
