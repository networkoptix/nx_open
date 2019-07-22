#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/abstract_authentication_manager.h>
#include <nx/utils/basic_factory.h>

namespace nx::cloud::storage::service{

struct CloudDb;
class Settings;

namespace http {

class CloudDbAuthenticationForwarder:
    public network::http::server::AbstractAuthenticationManager
{
public:
    CloudDbAuthenticationForwarder(const CloudDb& settings);

    virtual void authenticate(
        const network::http::HttpServerConnection& connection,
        const network::http::Request& request,
        network::http::server::AuthenticationCompletionHandler completionHandler) override;
};

//-------------------------------------------------------------------------------------------------
// CloudDbAuthenticationFactory

using CloudDbAuthenticationFactoryType =
    std::unique_ptr<network::http::server::AbstractAuthenticationManager>(const Settings&);

class CloudDbAuthenticationFactory:
    public nx::utils::BasicFactory<CloudDbAuthenticationFactoryType>
{
    using base_type = nx::utils::BasicFactory<CloudDbAuthenticationFactoryType>;

public:
    CloudDbAuthenticationFactory();

    static CloudDbAuthenticationFactory& instance();

private:
    std::unique_ptr<network::http::server::AbstractAuthenticationManager> defaultFactoryFunction(
        const Settings& settings);
};

} // namespace http
} // namespace nx::cloud::storage::service
