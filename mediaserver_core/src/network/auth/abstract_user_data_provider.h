/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_AUTH_ABSTRACT_USER_DATA_PROVIDER_H
#define NX_AUTH_ABSTRACT_USER_DATA_PROVIDER_H

#include <tuple>

#include <QtCore/QByteArray>

#include <nx/network/http/http_types.h>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>


class AbstractUserDataProvider
{
public:
    virtual ~AbstractUserDataProvider() {}

    //!Can find user or mediaserver
    virtual QnResourcePtr findResByName(const QByteArray& nxUserName) const = 0;
    //!Authorizes \a authorizationHeader with resource \a res
    virtual Qn::AuthResult authorize(
        const QnResourcePtr& res,
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader,
        nx_http::HttpHeaders* const responseHeaders) = 0;
    //!Authorizes \a authorizationHeader with any resource (user or server)
    /*!
        \return Resource is returned regardless of authentication result
    */
    virtual std::tuple<Qn::AuthResult, QnResourcePtr> authorize(
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader,
        nx_http::HttpHeaders* const responseHeaders) = 0;
};

#endif  //NX_AUTH_ABSTRACT_USER_DATA_PROVIDER_H
