// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <expected>

#include <QtCore/QString>

#include <nx/network/http/auth_tools.h>

namespace nx::vms::common {

/**
 * This class provides an interface to issue a new Session token.
 */
class NX_VMS_COMMON_API AbstractSessionTokenHelper
{
public:
    enum class RefreshError
    {
        userCancelled,
        internalError
    };

    using RefreshResult = std::expected<nx::network::http::AuthToken, RefreshError>;

    virtual ~AbstractSessionTokenHelper() = default;
    virtual RefreshResult refreshSession() = 0;
    virtual QString password() const = 0;
};

using SessionTokenHelperPtr = std::shared_ptr<AbstractSessionTokenHelper>;

} // namespace nx::vms::common
