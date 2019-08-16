#pragma once

#include <nx/network/http/server/abstract_authentication_manager.h>

#include <nx/cloud/db/api/auth_provider.h>
#include <nx/cloud/db/api/result_code.h>
#include <nx/utils/basic_factory.h>

namespace nx::cloud {

namespace db::api { class CdbClient; }

namespace storage::service {

namespace conf {

struct CloudDb;
class Settings;

} // namespace conf

namespace view::http {

class CloudDbAuthenticationForwarder:
    public network::http::server::AbstractAuthenticationManager
{
public:
    CloudDbAuthenticationForwarder(const conf::CloudDb& settings);

    virtual void authenticate(
        const network::http::HttpServerConnection& connection,
        const network::http::Request& request,
        network::http::server::AuthenticationCompletionHandler completionHandler) override;

private:
    struct AuthenticationRequest
    {
        std::unique_ptr<db::api::CdbClient> cdbClient;
        network::http::server::AuthenticationCompletionHandler handler;
    };

    void authenticateWithCloudDb(
        cloud::db::api::UserAuthorization userAuth,
        network::http::server::AuthenticationCompletionHandler completionHandler);

    db::api::CdbClient* createAuthenticationRequest(
        network::http::server::AuthenticationCompletionHandler completionHandler);

    AuthenticationRequest authenticationComplete(db::api::CdbClient* cdbClient);

    network::http::server::AuthenticationResult failedAuthenticationResult(
        db::api::ResultCode code,
        QByteArray reason);

private:
    const conf::CloudDb& m_settings;
    QnMutex m_mutex;
    std::map<db::api::CdbClient*, AuthenticationRequest> m_cdbContexts;
};

//-------------------------------------------------------------------------------------------------
// CloudDbAuthenticationFactory

using CloudDbAuthenticationFactoryType =
std::unique_ptr<network::http::server::AbstractAuthenticationManager>(const conf::Settings&);

class CloudDbAuthenticationFactory:
    public nx::utils::BasicFactory<CloudDbAuthenticationFactoryType>
{
    using base_type = nx::utils::BasicFactory<CloudDbAuthenticationFactoryType>;

public:
    CloudDbAuthenticationFactory();

    static CloudDbAuthenticationFactory& instance();

private:
    std::unique_ptr<network::http::server::AbstractAuthenticationManager> defaultFactoryFunction(
        const conf::Settings& settings);
};

} // namespace view::http
} // namespace storage::service
} // namespace nx::cloud
