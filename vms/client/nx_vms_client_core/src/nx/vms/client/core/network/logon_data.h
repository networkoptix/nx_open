// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/vms/api/data/user_data.h>

namespace nx::vms::client::core {

/**
 * Parameters to establish client-server connection.
 */
struct LogonData
{
    /**
     * Address of the Server.
     */
    nx::network::SocketAddress address;

    /**
     * User credentials. In case of cloud login password may be empty.
     */
    nx::network::http::Credentials credentials;

    /**
     * Type of the user.
     */
    nx::vms::api::UserType userType = nx::vms::api::UserType::local;

    /**
     * Id of the Server we expect to connect. Reqired to avoid passing stored credentials to another
     * Server on the same endpoint.
     */
    std::optional<QnUuid> expectedServerId;
};

} // namespace nx::vms::client::core
