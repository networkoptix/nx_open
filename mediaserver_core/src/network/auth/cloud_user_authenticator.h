/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MS_CLOUD_USER_AUTHENTICATOR_H
#define NX_MS_CLOUD_USER_AUTHENTICATOR_H

#include <memory>

#include "abstract_user_data_provider.h"


//!Add support for authentication using cloud account credentials
class CloudUserAuthenticator
:
    public AbstractUserDataProvider
{
public:
    /*!
        \param defaultAuthenticator Used to authenticate requests with local user credentials
    */
    CloudUserAuthenticator(std::unique_ptr<AbstractUserDataProvider> defaultAuthenticator);

    virtual QnResourcePtr findResByName(const QByteArray& nxUserName) const override;
    virtual Qn::AuthResult authorize(
        const QnResourcePtr& res,
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader) override;
    virtual std::tuple<Qn::AuthResult, QnResourcePtr> authorize(
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader) override;

private:
    std::unique_ptr<AbstractUserDataProvider> m_defaultAuthenticator;
};

#endif  //NX_MS_CLOUD_USER_AUTHENTICATOR_H
