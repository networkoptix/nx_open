/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_AUTH_ABSTRACT_USER_DATA_PROVIDER_H
#define NX_AUTH_ABSTRACT_USER_DATA_PROVIDER_H

#include <tuple>

#include <QtCore/QByteArray>

#include <core/resource/resource_fwd.h>
#include <utils/network/http/httptypes.h>


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
        const nx_http::header::Authorization& authorizationHeader) = 0;
    //!Authorizes \a authorizationHeader with any resource (user or server)
    virtual std::tuple<Qn::AuthResult, QnResourcePtr> authorize(
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader) = 0;
};

#endif  //NX_AUTH_ABSTRACT_USER_DATA_PROVIDER_H
