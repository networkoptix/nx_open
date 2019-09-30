#pragma once

#include <nx/network/http/server/authentication_dispatcher.h>
#include <nx/network/http/server/multi_endpoint_acceptor.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/http/server/http_server_htdigest_authentication_provider.h>

namespace nx::cloud::storage::service {

namespace conf { class Settings; }
namespace controller {

class BucketManager;
class Controller;
class StorageManager;

} // namespace controller

namespace view::http {

class Server
{
public:
    Server(const conf::Settings& settings, controller::Controller* controller);

    network::http::server::MultiEndpointAcceptor& server();
    network::http::server::rest::MessageDispatcher& messageDispatcher();

    bool bind();
    bool listen();
    void stop();

    std::vector<network::SocketAddress> httpEndpoints() const;
    std::vector<network::SocketAddress> httpsEndpoints() const;

private:
    void initializeHtdigestAuthenticator();
    void registerHtdigestAuthenticationPath(const std::string& regex);

    void registerAuthenticationManager(
        const std::string& regex,
        network::http::server::AbstractAuthenticationManager* authenticationManager);

    template<typename Handler, typename HandlerFunc>
    void registerRequestProcessor(
        const char* path,
        HandlerFunc handler,
        const nx::network::http::Method::ValueType& method);

    void registerStorageApiHandlers();
    void registerBucketApiHandlers();

private:
    /** Provides htdigest authentication*/
    struct HtdigestAuthenticator
    {
        nx::network::http::server::HtdigestAuthenticationProvider provider;
        nx::network::http::server::BaseAuthenticationManager manager;

        HtdigestAuthenticator(const std::string& htdigestPath):
            provider(htdigestPath),
            manager(&provider)
        {
        }
    };

    const conf::Settings& m_settings;
    controller::BucketManager* m_bucketManager = nullptr;
    controller::StorageManager* m_storageManager = nullptr;

    std::unique_ptr<network::http::server::AbstractAuthenticationManager>
        m_cloudDbAuthenticationForwarder;

    network::http::server::rest::MessageDispatcher m_messageDispatcher;
    network::http::server::AuthenticationDispatcher m_authenticationDispatcher;
    std::unique_ptr<HtdigestAuthenticator> m_htdigestAuthenticator;
    network::http::server::MultiEndpointAcceptor m_multiAddressServer;
};

} // namespace view::http
} // namespace nx::cloud::storage::service