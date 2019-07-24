#include "server.h"

#include <nx/network/cloud/storage/service/api/request_paths.h>
#include <nx/network/url/url_parse_helper.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/controller/controller.h"
#include "nx/cloud/storage/service/controller/bucket_manager.h"
#include "nx/cloud/storage/service/controller/storage_manager.h"
#include "cloud_db_authentication_manager.h"
#include "request_handler.h"

namespace nx::cloud::storage::service::view::http {

namespace {

static constexpr char kStorageIdParam[] = "id";

} // namespace

Server::Server(const conf::Settings& settings, controller::Controller* controller):
    m_settings(settings),
    m_bucketManager(controller->bucketManager()),
    m_storageManager(controller->storageManager()),
    m_cloudDBAuthenticationForwarder(CloudDbAuthenticationFactory::instance().create(settings)),
    m_multiAddressServer(
        &m_authenticationDispatcher,
        &m_messageDispatcher)
{
    // Adding "/storages/" and "/storage/{id}" to cloud db authentication forwarding.
    registerAuthenticationManager(
        std::string(api::kStoragePrefix) + ".*",
        m_cloudDBAuthenticationForwarder.get());

    registerBucketApiHandlers();
    registerStorageApiHandlers();
}

network::http::server::MultiEndpointAcceptor& Server::server()
{
    return m_multiAddressServer;
}

network::http::server::rest::MessageDispatcher& Server::messageDispatcher()
{
    return m_messageDispatcher;
}

bool Server::bind()
{
    return m_multiAddressServer.bind(
        m_settings.http().endpoints,
        m_settings.http().sslEndpoints);
}

bool Server::listen()
{
    return m_multiAddressServer.listen(
        m_settings.http().tcpBacklogSize);
}

void Server::stop()
{
    m_multiAddressServer.pleaseStopSync();
}

std::vector<network::SocketAddress> Server::httpEndpoints() const
{
    return m_multiAddressServer.endpoints();
}

std::vector<network::SocketAddress> Server::httpsEndpoints() const
{
    return m_multiAddressServer.sslEndpoints();
}

void Server::registerAuthenticationManager(
    const std::string& regex,
    network::http::server::AbstractAuthenticationManager* authenticationManager)
{
    m_authenticationDispatcher.add(std::regex(regex), authenticationManager);
    NX_VERBOSE(this, "Registered path regex: %1 for HTTP authentication", regex);
}

void Server::registerStorageApiHandlers()
{
    using namespace std::placeholders;
    using namespace nx::network::http::server::rest;

    using AddStorageHandler = RequestHandler<api::AddStorageRequest, api::Storage>;
    m_messageDispatcher.registerRequestProcessor<AddStorageHandler>(
        api::kStorages,
        [this]()
        {
            return std::make_unique<AddStorageHandler>(
                std::bind(&controller::StorageManager::addStorage, m_storageManager, _1, _2));
        },
        network::http::Method::put);

    using ReadStorageHandler =
        RequestHandler<void, api::Storage, RestArgFetcher<kStorageIdParam>>;
    m_messageDispatcher.registerRequestProcessor<ReadStorageHandler>(
        api::kStorageId,
        [this]()
        {
            return std::make_unique<ReadStorageHandler>(
                std::bind(&controller::StorageManager::readStorage, m_storageManager, _1, _2));
        },
        network::http::Method::get);

    using RemoveStorageHandler = RequestHandler<void, void, RestArgFetcher<kStorageIdParam>>;
    m_messageDispatcher.registerRequestProcessor<RemoveStorageHandler>(
        api::kStorageId,
        [this]()
        {
            return std::make_unique<RemoveStorageHandler>(
                std::bind(&controller::StorageManager::removeStorage, m_storageManager, _1, _2));
        },
        network::http::Method::delete_);
}

void Server::registerBucketApiHandlers()
{
    // TODO
}

} // namespace nx::cloud::storage::service::view::http
