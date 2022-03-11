// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/vms/api/data/user_data.h>

namespace nx::vms::client::core {

struct ConnectionInfo
{
    using UserType = nx::vms::api::UserType;

    nx::network::SocketAddress address;
    nx::network::http::Credentials credentials;
    UserType userType = UserType::local;

    /**
     * Allow user interaction in the UI thread (like displaying dialog about certifcate mismatch).
     */
    bool userInteractionAllowed = true;
};

} // namespace nx::vms::client::core
