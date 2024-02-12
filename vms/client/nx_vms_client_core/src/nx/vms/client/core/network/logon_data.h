// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

namespace nx::vms::client::core {

/**
 * Parameters to establish client-server connection.
 */
struct NX_VMS_CLIENT_CORE_API LogonData
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

    using Purpose = nx::vms::common::ServerCompatibilityValidator::Purpose;
    /**
     * Connection purpose affects the way, how Server compatibility is checked.
     */
    Purpose purpose = Purpose::connect;

    /**
     * Id of the Server we expect to connect. Reqired to avoid passing stored credentials to
     * another Server on the same endpoint.
     */
    std::optional<nx::Uuid> expectedServerId = std::nullopt;

    /**
     * Version of the server we expect to connect. Allows to simplify the connection scenario.
     */
    std::optional<nx::utils::SoftwareVersion> expectedServerVersion;

    /**
     * Cloud ID of the System we expect to connect. Allows to simplify the connection scenario.
     */
    std::optional<QString> expectedCloudSystemId;

    /**
     * Allow user interaction in the UI thread (like displaying dialog about certifcate mismatch).
     */
    bool userInteractionAllowed = true;
};

} // namespace nx::vms::client::core
