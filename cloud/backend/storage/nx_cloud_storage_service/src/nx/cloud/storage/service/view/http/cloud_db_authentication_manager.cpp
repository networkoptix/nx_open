#include "cloud_db_authentication_manager.h"

#include "nx/cloud/storage/service/settings.h"

namespace nx::cloud::storage::service::view::http {

using namespace nx::network::http;

CloudDbAuthenticationForwarder::CloudDbAuthenticationForwarder(const conf::CloudDb& /*settings*/)
{
    // TODO add cloud db url to settings
}

void CloudDbAuthenticationForwarder::authenticate(
    const HttpServerConnection& /*connection*/,
    const Request& /*request*/,
    server::AuthenticationCompletionHandler completionHandler)
{
    // TODO forward authentication to cloud db
    completionHandler(server::SuccessfulAuthenticationResult());
}

//-------------------------------------------------------------------------------------------------
// CloudDbAuthenticationFactory

CloudDbAuthenticationFactory::CloudDbAuthenticationFactory():
    base_type(std::bind(
        &CloudDbAuthenticationFactory::defaultFactoryFunction,
        this,
        std::placeholders::_1))
{
}

CloudDbAuthenticationFactory& CloudDbAuthenticationFactory::instance()
{
    static CloudDbAuthenticationFactory factory;
    return factory;
}

std::unique_ptr<server::AbstractAuthenticationManager>
    CloudDbAuthenticationFactory::defaultFactoryFunction(
        const conf::Settings& settings)
{
    return std::make_unique<CloudDbAuthenticationForwarder>(settings.cloudDb());
}

} // namespace nx::cloud::storage::service::view::http
