#pragma once

#include <nx/network/http/server/authentication_dispatcher.h>
#include <nx/network/http/server/multi_endpoint_acceptor.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

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
    const conf::Settings& m_settings;
    controller::BucketManager* m_bucketManager = nullptr;
    controller::StorageManager* m_storageManager = nullptr;

    std::unique_ptr<network::http::server::AbstractAuthenticationManager>
        m_cloudDBAuthenticationForwarder;

    network::http::server::rest::MessageDispatcher m_messageDispatcher;
    network::http::server::AuthenticationDispatcher m_authenticationDispatcher;
    network::http::server::MultiEndpointAcceptor m_multiAddressServer;
};

} // namespace view::http
} // namespace nx::cloud::storage::service