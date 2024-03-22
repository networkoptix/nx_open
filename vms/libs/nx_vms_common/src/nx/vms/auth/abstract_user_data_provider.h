// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include <QtCore/QByteArray>

#include <core/resource/resource_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/auth_result.h>
#include <nx/string.h>

namespace nx {
namespace vms {
namespace auth {

class AbstractUserDataProvider
{
public:
    using AuthResult = nx::network::rest::AuthResult;

    virtual ~AbstractUserDataProvider() = default;

    /**
     * Can find user or mediaserver.
     */
    virtual std::pair<QnResourcePtr, bool /*hasClash*/> findResByName(
        const nx::String& nxUserName) const = 0;

    /**
     * Authorizes authorizationHeader with resource res.
     */
    virtual AuthResult authorize(
        const QnResourcePtr& res,
        const nx::network::http::Method& method,
        const nx::network::http::header::Authorization& authorizationHeader,
        nx::network::http::HttpHeaders* const responseHeaders) = 0;

    /**
     * Authorizes authorizationHeader with any resource (user or server).
     * @return Resource is returned regardless of authentication result.
     */
    virtual std::tuple<AuthResult, QnResourcePtr> authorize(
        const nx::network::http::Method& method,
        const nx::network::http::header::Authorization& authorizationHeader,
        nx::network::http::HttpHeaders* const responseHeaders) = 0;
};

} // namespace auth
} // namespace vms
} // namespace nx
