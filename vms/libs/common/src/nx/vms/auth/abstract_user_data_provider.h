#pragma once

#include <tuple>

#include <QtCore/QByteArray>

#include <nx/network/http/http_types.h>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace auth {

class AbstractUserDataProvider
{
public:
    virtual ~AbstractUserDataProvider() = default;

    /**
     * Can find user or mediaserver.
     */
    virtual QnResourcePtr findResByName(const QByteArray& nxUserName) const = 0;

    /**
     * Authorizes authorizationHeader with resource res.
     */
    virtual Qn::AuthResult authorize(
        const QnResourcePtr& res,
        const nx::network::http::Method::ValueType& method,
        const nx::network::http::header::Authorization& authorizationHeader,
        nx::network::http::HttpHeaders* const responseHeaders) = 0;

    /**
     * Authorizes authorizationHeader with any resource (user or server).
     * @return Resource is returned regardless of authentication result.
     */
    virtual std::tuple<Qn::AuthResult, QnResourcePtr> authorize(
        const nx::network::http::Method::ValueType& method,
        const nx::network::http::header::Authorization& authorizationHeader,
        nx::network::http::HttpHeaders* const responseHeaders) = 0;
};

} // namespace auth
} // namespace vms
} // namespace nx
