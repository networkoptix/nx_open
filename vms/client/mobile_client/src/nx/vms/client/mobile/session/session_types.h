// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <nx/vms/client/core/network/remote_connection_fwd.h>

namespace nx::vms::client::mobile::session {

using ConnectionCallback = std::function<void(
    std::optional<core::RemoteConnectionErrorCode> /*errorCode*/)>;

} // namespace nx::vms::client::mobile::session
