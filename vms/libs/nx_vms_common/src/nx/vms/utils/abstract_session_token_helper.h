// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/network/http/auth_tools.h>

namespace nx::vms::common {

/**
 * This class provides an interface to issue a new Session token.
 */
class NX_VMS_COMMON_API AbstractSessionTokenHelper
{
public:
    virtual ~AbstractSessionTokenHelper() = default;

    /* Return access token. */
    virtual std::optional<nx::network::http::AuthToken> refreshSession() = 0;

    virtual QString password() const = 0;
};

using SessionTokenHelperPtr = std::shared_ptr<AbstractSessionTokenHelper>;

} // namespace nx::vms::common
