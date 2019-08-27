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

namespace controller::cloud_db {

class AuthenticationForwarder:
    public network::http::server::AbstractAuthenticationManager
{
public:
    AuthenticationForwarder(const conf::CloudDb& settings);

    virtual void authenticate(
        const network::http::HttpServerConnection& connection,
        const network::http::Request& request,
        network::http::server::AuthenticationCompletionHandler completionHandler) override;

private:
    struct RequestContext
    {
        std::unique_ptr<db::api::CdbClient> cdbClient;
        network::http::server::AuthenticationCompletionHandler handler;
    };

    void authenticateWithCloudDb(
        cloud::db::api::UserAuthorization userAuth,
        network::http::server::AuthenticationCompletionHandler completionHandler);

    db::api::CdbClient* createAuthenticationRequest(
        network::http::server::AuthenticationCompletionHandler completionHandler);

    RequestContext takeRequestContext(db::api::CdbClient* cdbClient);

    network::http::server::AuthenticationResult prepareFailedAuthenticationResult(
        db::api::ResultCode code,
        QByteArray reason);

private:
    const conf::CloudDb& m_settings;
    QnMutex m_mutex;
    std::map<db::api::CdbClient*, RequestContext> m_cdbContexts;
};

//-------------------------------------------------------------------------------------------------
// AuthenticationFactory

using FactoryType =
std::unique_ptr<network::http::server::AbstractAuthenticationManager>(const conf::Settings&);

class AuthenticationManagerFactory:
    public nx::utils::BasicFactory<FactoryType>
{
    using base_type = nx::utils::BasicFactory<FactoryType>;

public:
    AuthenticationManagerFactory();

    static AuthenticationManagerFactory& instance();

private:
    std::unique_ptr<network::http::server::AbstractAuthenticationManager> defaultFactoryFunction(
        const conf::Settings& settings);
};

} // namespace controller::cloud_db
} // namespace storage::service
} // namespace nx::cloud
