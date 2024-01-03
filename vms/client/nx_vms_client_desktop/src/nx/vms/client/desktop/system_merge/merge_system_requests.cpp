// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "merge_system_requests.h"

#include <memory>

#include <QtCore/QPointer>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>
#include <nx/utils/serialization/format.h>
#include <nx/vms/client/core/network/network_manager.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/common/api/helpers/parser_helper.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/ec_api_common.h>

using NetworkManager = nx::vms::client::core::NetworkManager;

namespace {

struct Request
{
    nx::utils::Url url;
    std::unique_ptr<nx::network::http::AsyncClient> client;
};

template<typename Data>
rest::ErrorOrData<Data> parseResultOrData(
    const NetworkManager::Response& response)
{
    if (response.statusLine.statusCode == nx::network::http::StatusCode::ok)
    {
        Data data;
        if (QJson::deserialize(response.messageBody, &data))
            return data;
        return nx::network::rest::Result::notImplemented();
    }

    NX_DEBUG(
        typeid(nx::vms::client::desktop::MergeSystemRequestsManager),
        "Unexpected response, error code: %1, HTTP status: %2",
        response.errorCode,
        response.statusLine.statusCode);

    return nx::vms::common::api::parseRestResult(
        static_cast<nx::network::http::StatusCode::Value>(response.statusLine.statusCode),
        Qn::SerializationFormat::json,
        response.messageBody);
}

void addRequestData(Request* request, nx::Buffer&& data)
{
    request->client->addAdditionalHeader(
        nx::network::http::header::kAccept,
        nx::network::http::header::ContentType::kJson);

    if (!data.empty())
    {
        auto messageBody = std::make_unique<nx::network::http::BufferSource>(
            Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
            std::move(data));
        request->client->setRequestBody(std::move(messageBody));
    }
}

Request makeDirectRequest(
    QnMediaServerResourcePtr proxy,
    nx::network::ssl::AdapterFunc proxyAdapterFunc,
    const nx::network::http::Credentials& credentials,
    const std::string& path,
    nx::Buffer data)
{
    Request request;
    request.url = proxy->getApiUrl();
    request.url.setPath(path);

    request.client = std::make_unique<nx::network::http::AsyncClient>(std::move(proxyAdapterFunc));
    addRequestData(&request, std::move(data));
    request.client->setCredentials(credentials);

    return request;
}

Request makeConnectedRequest(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    nx::network::ssl::AdapterFunc targetAdapterFunc,
    const std::string& path,
    const std::string& token,
    nx::Buffer data)
{
    Request request;
    request.url.setPath(path);

    request.client = std::make_unique<nx::network::http::AsyncClient>(
        targetAdapterFunc(std::move(socket)), targetAdapterFunc);

    addRequestData(&request, std::move(data));

    if (!token.empty())
        request.client->setCredentials(nx::network::http::BearerAuthToken(token));

    return request;
}

std::unique_ptr<nx::network::http::AsyncClient> makeConnectClient(
    QnMediaServerResourcePtr proxy, nx::network::ssl::AdapterFunc proxyAdapterFunc)
{
    // While making CONNECT request, proxy certificate is being verified.
    auto httpConnectClient =
        std::make_unique<nx::network::http::AsyncClient>(std::move(proxyAdapterFunc));

    nx::vms::client::core::RemoteConnectionAware connectionHelper;
    auto connection = connectionHelper.connection();
    if (NX_ASSERT(connection))
        httpConnectClient->setProxyCredentials(connection->credentials());
    NetworkManager::setDefaultTimeouts(httpConnectClient.get());
    return httpConnectClient;
}

std::unique_ptr<nx::network::AbstractStreamSocket> takeSocketSync(
    nx::network::http::AsyncClient* client)
{
    std::unique_ptr<nx::network::AbstractStreamSocket> socket;
    nx::utils::promise<void> socketTakenPromise;
    client->dispatch(
        [client, &socket, &socketTakenPromise]()
        {
            socket = client->takeSocket();
            socketTakenPromise.set_value();
        });
    socketTakenPromise.get_future().wait();

    return socket;
}

} // namespace

namespace nx::vms::client::desktop {

class MergeSystemRequestsManager::Private
{
public:
    Private(QObject* parent, const std::string& locale):
        m_parent(parent),
        m_locale(locale),
        m_networkManager(std::make_unique<NetworkManager>())
    {}

    void setupClient(nx::network::http::AsyncClient* client)
    {
        NX_CRITICAL(client);
        client->addAdditionalHeader(Qn::kAcceptLanguageHeader, m_locale);
        NetworkManager::setDefaultTimeouts(client);
    }

    template<typename Data>
    void doRequest(nx::network::http::Method method, Request request, Callback<Data> callback)
    {
        setupClient(request.client.get());
        m_networkManager->doRequest(method, std::move(request.client),
            request.url,
            m_parent,
            [callback = std::move(callback)](NetworkManager::Response response)
            {
                callback(parseResultOrData<Data>(response));
            });
    }

    template<typename Data>
    void doRequest(
        QnMediaServerResourcePtr proxy,
        nx::network::ssl::AdapterFunc proxyAdapterFunc,
        nx::network::SocketAddress target,
        nx::network::ssl::AdapterFunc targetAdapterFunc,
        nx::network::http::Method method,
        const std::string& path,
        const std::string& token,
        nx::Buffer data,
        Callback<Data> callback)
    {
        auto onConnect =
            [this,
            callback = std::move(callback),
            path,
            method,
            token,
            data = std::move(data),
            targetAdapterFunc = std::move(targetAdapterFunc)](
                std::shared_ptr<nx::network::http::AsyncClient> client)
            {
                if (!client || client->failed() || !client->response()
                    || (client->response()->statusLine.statusCode ==
                        nx::network::http::StatusCode::undefined))
                {
                    NX_DEBUG(this, "Can't connect to proxy, IO error.");
                    callback(nx::network::rest::Result::serviceUnavailable());
                    return;
                }

                const auto statusCode = client->response()->statusLine.statusCode;
                if (statusCode != nx::network::http::StatusCode::ok)
                {
                    NX_DEBUG(
                        this, "Can't connect to proxy, unexpected HTTP status: %1", statusCode);
                    const auto result = nx::network::rest::Result{
                        nx::network::rest::Result::errorFromHttpStatus(statusCode)};
                    callback(result);
                    return;
                }

                // Async client's socket is taken in it's own AIO-thread to avoid deadlock.
                // Async client itself created and destroyed in UI-thread, so we can safely use
                // shared pointer to it.
                Request request = makeConnectedRequest(
                    takeSocketSync(client.get()),
                    std::move(targetAdapterFunc),
                    path,
                    token,
                    std::move(data));
                doRequest(method, std::move(request), std::move(callback));
            };

        auto connectClient = makeConnectClient(proxy, std::move(proxyAdapterFunc));
        setupClient(connectClient.get());

        m_networkManager->doConnect(
            std::move(connectClient),
            proxy->getApiUrl(),
            target.toString(),
            m_parent,
            std::move(onConnect));
    }

    void pleaseStopSync()
    {
        m_networkManager->pleaseStopSync();
    }

private:
    QObject* m_parent;
    std::string m_locale;
    std::unique_ptr<NetworkManager> m_networkManager;
};

MergeSystemRequestsManager::MergeSystemRequestsManager(
    QObject* parent,
    const std::string& locale)
    :
    d(new Private(parent, locale))
{
}

MergeSystemRequestsManager::~MergeSystemRequestsManager()
{
    d->pleaseStopSync();
}

void MergeSystemRequestsManager::getTargetInfo(
    QnMediaServerResourcePtr proxy,
    nx::network::ssl::AdapterFunc proxyAdapterFunc,
    const nx::network::SocketAddress& target,
    nx::network::ssl::AdapterFunc targetAdapterFunc,
    Callback<nx::vms::api::ServerInformation> callback)
{
    NX_DEBUG(this, "Requesting info from %1, via %2", target, proxy);
    d->doRequest(
        proxy,
        std::move(proxyAdapterFunc),
        target,
        std::move(targetAdapterFunc),
        nx::network::http::Method::get,
        "/rest/v1/servers/this/info",
        {} /*token*/,
        {} /*data*/,
        std::move(callback));
}

void MergeSystemRequestsManager::createTargetSession(
    QnMediaServerResourcePtr proxy,
    nx::network::ssl::AdapterFunc proxyAdapterFunc,
    const nx::network::SocketAddress& target,
    nx::network::ssl::AdapterFunc targetAdapterFunc,
    const nx::vms::api::LoginSessionRequest& request,
    Callback<nx::vms::api::LoginSession> callback)
{
    NX_DEBUG(
        this, "Creating session for user %1 on %2 via %3", request.username, target, proxy);
    d->doRequest(
        proxy,
        std::move(proxyAdapterFunc),
        target,
        std::move(targetAdapterFunc),
        nx::network::http::Method::post,
        "/rest/v1/login/sessions",
        {} /*token*/,
        QJson::serialized(request),
        std::move(callback));
}

void MergeSystemRequestsManager::getTargetLicenses(
    QnMediaServerResourcePtr proxy,
    nx::network::ssl::AdapterFunc proxyAdapterFunc,
    const nx::network::SocketAddress& target,
    const std::string& authToken,
    nx::network::ssl::AdapterFunc targetAdapterFunc,
    Callback<nx::vms::api::LicenseDataList> callback)
{
    NX_DEBUG(this, "Requesting licenses for %2 via %3", target, proxy);
    d->doRequest(
        proxy,
        std::move(proxyAdapterFunc),
        target,
        std::move(targetAdapterFunc),
        nx::network::http::Method::get,
        "/rest/v1/licenses",
        authToken,
        {} /*data*/,
        std::move(callback));
}

void MergeSystemRequestsManager::mergeSystem(
    QnMediaServerResourcePtr proxy,
    nx::network::ssl::AdapterFunc proxyAdapterFunc,
    const nx::network::http::Credentials& credentials,
    const nx::vms::api::SiteMergeData& data,
    Callback<nx::vms::api::MergeStatusReply> callback)
{
    NX_DEBUG(this, "Merging target %1 to %2", data.remoteEndpoint, proxy);
    NX_ASSERT(!credentials.authToken.empty(), "Credentials are required");
    Request request = makeDirectRequest(
        proxy,
        std::move(proxyAdapterFunc),
        credentials,
        "/rest/v1/system/merge",
        QJson::serialized(data));
    d->doRequest(nx::network::http::Method::post, std::move(request), std::move(callback));
}

} // namespace nx::vms::client::desktop
