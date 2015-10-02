/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#include "cloud_user_authenticator.h"


CloudUserAuthenticator::CloudUserAuthenticator(
    std::unique_ptr<AbstractUserDataProvider> defaultAuthenticator)
:
    m_defaultAuthenticator(std::move(defaultAuthenticator))
{
}

QnResourcePtr CloudUserAuthenticator::findResByName(const QByteArray& nxUserName) const
{
    //TODO #ak
    return m_defaultAuthenticator->findResByName(nxUserName);
}

Qn::AuthResult CloudUserAuthenticator::authorize(
    const QnResourcePtr& res,
    const nx_http::Method::ValueType& method,
    const nx_http::header::Authorization& authorizationHeader)
{
    //TODO #ak
    return m_defaultAuthenticator->authorize(res, method, authorizationHeader);
}

std::tuple<Qn::AuthResult, QnResourcePtr> CloudUserAuthenticator::authorize(
    const nx_http::Method::ValueType& method,
    const nx_http::header::Authorization& authorizationHeader)
{
    auto res = findResByName(authorizationHeader.userid());
    if (!res)
        return std::make_tuple(Qn::Auth_WrongLogin, QnResourcePtr());
    return std::make_tuple(
        authorize(res, method, authorizationHeader),
        res);
}
