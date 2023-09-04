// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/user_model.h>

namespace nx::vms::client::core {

/**
 * Information about an established client-server connection.
 */
struct ConnectionInfo
{
    nx::network::SocketAddress address;
    nx::network::http::Credentials credentials;
    nx::vms::api::UserType userType = nx::vms::api::UserType::local;

    /** User info for the pre-6.0 Systems where old permissions model is implemented. */
    std::optional<nx::vms::api::UserModelV1> compatibilityUserModel;

    bool isCloud() const
    {
        return userType == nx::vms::api::UserType::cloud;
    }

    bool isTemporary() const
    {
        return userType == nx::vms::api::UserType::temporaryLocal;
    }
};

} // namespace nx::vms::client::core
