#include "server.h"

#include <nx/cloud/storage/service/api/request_paths.h>
#include <nx/network/url/url_parse_helper.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/controller/controller.h"
#include "nx/cloud/storage/service/controller/bucket_manager.h"
#include "nx/cloud/storage/service/controller/storage_manager.h"
#include "nx/cloud/storage/service/controller/cloud_db/resource.h"
#include "nx/cloud/storage/service/controller/cloud_db/authentication_manager.h"
#include "request_handler.h"

namespace nx::cloud::storage::service::view::http {

namespace {

static constexpr char kStorageId[] = "storageId";
static constexpr char kSystemId[] = "systemId";
static constexpr char kBucketName[] = "bucketName";

} // namespace

Server::Server(const conf::Settings& settings, controller::Controller* controller):
    m_settings(settings),
    m_bucketManager(&controller->bucketManager()),
    m_storageManager(&controller->storageManager()),
    m_cloudDbAuthenticationForwarder(
        controller::cloud_db::AuthenticationManagerFactory::instance().create(settings)),
    m_multiAddressServer(
        &m_authenticationDispatcher,
        &m_messageDispatcher)
{
    initializeHtdigestAuthenticator();

    // Adding "/storages/" and "/storage/{storageId}" to cloud db authentication forwarding.
    registerAuthenticationManager(
        std::string(api::kStoragePrefix) + ".*",
        m_cloudDbAuthenticationForwarder.get());

    // Adding "/aws_buckets/" and "/aws_buckets/{bucketName}" to htdigest authentication
    registerHtdigestAuthenticationPath(std::string(api::kAwsBucketsPrefix) + ".*");

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

void Server::initializeHtdigestAuthenticator()
{
    if (m_settings.http().htdigestPath.empty())
        return;

    NX_INFO(this, "Initializing htdigest authenticator, htdigest file path is %1",
        m_settings.http().htdigestPath);

    m_htdigestAuthenticator =
        std::make_unique<HtdigestAuthenticator>(m_settings.http().htdigestPath);
}

void Server::registerHtdigestAuthenticationPath(const std::string& regex)
{
    if (!m_htdigestAuthenticator)
        return;

    NX_INFO(this, "Registering path %1 for htdigest authentication", api::kAwsBucketsPrefix);

    registerAuthenticationManager(regex, &m_htdigestAuthenticator->manager);
}

void Server::registerAuthenticationManager(
    const std::string& regex,
    network::http::server::AbstractAuthenticationManager* authenticationManager)
{
    m_authenticationDispatcher.add(std::regex(regex), authenticationManager);
    NX_INFO(this, "Registered path regex: %1 for HTTP authentication, authenticationManager: %2",
        regex, authenticationManager);
}

void Server::registerStorageApiHandlers()
{
    using namespace std::placeholders;
    using namespace nx::network::http::server::rest;

    registerRequestProcessor<
        RequestHandler<
            api::AddStorageRequest, api::Storage, AuthInfoFetcher, ClientEndpointFetcher>>(
                api::kStorages,
                std::bind(&controller::StorageManager::addStorage, m_storageManager, _1, _2, _3, _4),
                network::http::Method::put);

    registerRequestProcessor<
        RequestHandler<void, api::Storage, AuthInfoFetcher, RestArgFetcher<kStorageId>>>(
            api::kStorage,
            std::bind(&controller::StorageManager::readStorage, m_storageManager, _1, _2, _3),
            network::http::Method::get);

    registerRequestProcessor<
        RequestHandler<void, void, AuthInfoFetcher, RestArgFetcher<kStorageId>>>(
            api::kStorage,
            std::bind(&controller::StorageManager::removeStorage, m_storageManager, _1, _2, _3),
            network::http::Method::delete_);

    registerRequestProcessor<
        RequestHandler<void, api::StorageCredentials, AuthInfoFetcher, RestArgFetcher<kStorageId>>>(
            api::kStorageCredentials,
            std::bind(&controller::StorageManager::getCredentials, m_storageManager, _1, _2, _3),
            network::http::Method::get);

    registerRequestProcessor<
        RequestHandler<api::AddSystemRequest, api::System, AuthInfoFetcher, RestArgFetcher<kStorageId>>>(
            api::kSystems,
            std::bind(&controller::StorageManager::addSystem, m_storageManager, _1, _2, _3, _4),
            network::http::Method::put);

    registerRequestProcessor<
        RequestHandler<
            void, void, AuthInfoFetcher, RestArgFetcher<kStorageId>, RestArgFetcher<kSystemId>>>(
                api::kSystem,
                std::bind(&controller::StorageManager::removeSystem, m_storageManager, _1, _2, _3, _4),
                network::http::Method::delete_);
}

void Server::registerBucketApiHandlers()
{
    using namespace std::placeholders;
    using namespace nx::network::http::server::rest;

    registerRequestProcessor<RequestHandler<api::AddBucketRequest, api::Bucket>>(
        api::kAwsBuckets,
        std::bind(&controller::BucketManager::addBucket, m_bucketManager, _1, _2),
        network::http::Method::put);

    registerRequestProcessor<RequestHandler<void, api::Buckets>>(
        api::kAwsBuckets,
        std::bind(&controller::BucketManager::listBuckets, m_bucketManager, _1),
        network::http::Method::get);

    registerRequestProcessor<RequestHandler<void, void, RestArgFetcher<kBucketName>>>(
        api::kAwsBucket,
        std::bind(&controller::BucketManager::removeBucket, m_bucketManager, _1, _2),
        network::http::Method::delete_);
}

template<typename Handler, typename HandlerFunc>
void Server::registerRequestProcessor(
    const char* path,
    HandlerFunc handlerFunc,
    const nx::network::http::Method::ValueType& method)
{
    m_messageDispatcher.registerRequestProcessor<Handler>(
        path,
        [handlerFunc = std::move(handlerFunc)]()
        {
            return std::make_unique<Handler>(std::move(handlerFunc));
        },
        method);
}

} // namespace nx::cloud::storage::service::view::http
