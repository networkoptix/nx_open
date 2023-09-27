// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_manager.h"

#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_async_client.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/auth/auth_result.h>
#include <nx_ec/ec_api_common.h>

using AsyncClient = nx::network::http::AsyncClient;
using Method = nx::network::http::Method;

namespace nx::vms::client::core {

namespace {

int generateRequestId()
{
    static std::atomic<int> requestId(0);
    return ++requestId;
}

ec2::ErrorCode parseErrorCode(const nx::network::http::Response* response)
{
    switch (response->statusLine.statusCode)
    {
        case nx::network::http::StatusCode::ok:
            return ec2::ErrorCode::ok;
        case nx::network::http::StatusCode::unauthorized:
            return ec2::ErrorCode::unauthorized;
        case nx::network::http::StatusCode::notImplemented:
            return ec2::ErrorCode::unsupported;
        case nx::network::http::StatusCode::forbidden:
            return ec2::ErrorCode::forbidden;
        default:
            return ec2::ErrorCode::serverError;
    }
}

NetworkManager::Response parseResponse(AsyncClient* client)
{
    NetworkManager::Response result;

    if (client->failed() || !client->response())
    {
        result.errorCode = ec2::ErrorCode::ioError;
        return result;
    }

    result.statusLine = client->response()->statusLine;
    result.headers = client->response()->headers;
    result.messageBody = client->fetchMessageBodyBuffer();
    result.errorCode = parseErrorCode(client->response());
    result.contentType = client->contentType();
    return result;
}

/** Request context. */
struct Context
{
    std::unique_ptr<AsyncClient> client;
    QMetaObject::Connection connection;

    Context() = default;
    Context(Context&&) noexcept = default;
    Context& operator=(Context&&) noexcept = default;

    ~Context()
    {
        QObject::disconnect(connection);
    }
};

} // namespace

struct NetworkManager::Private
{
    /** Mutex for guarding requests list. */
    nx::Mutex mutex;

    /** List of sent requests. */
    std::map<int, Context> requests;

    std::optional<Context> takeContext(int requestId)
    {
        NX_MUTEX_LOCKER lk(&mutex);
        auto iter = requests.find(requestId);
        if (iter == requests.end())
            return std::nullopt;

        Context context = std::move(iter->second);
        requests.erase(iter);
        lk.unlock();

        return context;
    }
};

NetworkManager::NetworkManager(QObject* parent):
    base_type(parent),
    d(new Private())
{
    qRegisterMetaType<NetworkManager::Response>();
    qRegisterMetaType<std::shared_ptr<nx::network::http::AsyncClient>>();
}

NetworkManager::~NetworkManager()
{
    pleaseStopSync();
}

void NetworkManager::pleaseStopSync()
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    while (!d->requests.empty())
    {
        Context context = std::move(d->requests.begin()->second);
        context.connection = {};
        d->requests.erase(d->requests.begin());
        lk.unlock(); //< Must unlock mutex to avoid deadlock with http completion handler.
        context.client->pleaseStopSync();
        // It is garanteed that no http event handler is running currently and no handler will
        // be called.
        lk.relock();
    }
}

void NetworkManager::setDefaultTimeouts(AsyncClient* client)
{
    using namespace std::chrono;
    static const milliseconds kResponseReadTimeout = 30s;
    static const milliseconds kSendTimeout = 20s;
    static const milliseconds kMessageBodyReadTimeout = 1min;

    client->setResponseReadTimeout(kResponseReadTimeout);
    client->setSendTimeout(kSendTimeout);
    client->setMessageBodyReadTimeout(kMessageBodyReadTimeout);
}

void NetworkManager::doRequest(
    nx::network::http::Method method,
    std::unique_ptr<nx::network::http::AsyncClient> request,
    nx::utils::Url url,
    QObject* owner,
    RequestCallback callback,
    Qt::ConnectionType connectionType)
{
    NX_VERBOSE(this, "[%1] %2", method, url);
    NX_ASSERT(url.userName().isEmpty(), "Credentials must be set explicitly");
    if (request->credentials().authToken.empty() && !url.userName().isEmpty())
    {
        request->setCredentials(nx::network::http::PasswordCredentials(
            url.userName().toStdString(), url.password().toStdString()));
        url.setUserName(QString());
        url.setPassword(QString());
    }

    const int reqId = generateRequestId();

    auto onHttpDone =
        [this, reqId]()
        {
            if (auto context = d->takeContext(reqId))
            {
                Response response = parseResponse(context->client.get());
                emit onRequestDone(reqId, std::move(response), QPrivateSignal());
                // Here context will be destroyed and connection will be broken.
            }
        };
    request->setOnDone(onHttpDone);

    Context context;
    context.client = std::move(request);
    context.connection = connect(this, &NetworkManager::onRequestDone, owner,
        [callback = std::move(callback), reqId](int requestId, Response response) mutable
        {
            if (reqId == requestId)
                callback(response);
        },
        connectionType);

    // Sending request under mutex to be sure reply is not handled before we store context.
    NX_MUTEX_LOCKER lk(&d->mutex);
    context.client->doRequest(method, url);
    d->requests.emplace(reqId, std::move(context));
}

void NetworkManager::doConnect(
    std::unique_ptr<nx::network::http::AsyncClient> client,
    const nx::utils::Url& proxyUrl,
    const std::string& targetHost,
    QObject* owner,
    ConnectCallback callback,
    Qt::ConnectionType connectionType)
{
    const int reqId = generateRequestId();
    auto onConnect =
        [this, reqId]()
        {
            if (auto context = d->takeContext(reqId))
            {
                std::shared_ptr<nx::network::http::AsyncClient> clientPtr = std::move(context->client);
                emit onConnectDone(reqId, clientPtr, QPrivateSignal());
                // Connection will be processed in callback.
            }
        };

    client->setOnResponseReceived(onConnect);
    // onDone callback can be used as error handler in the case when no response is received.
    client->setOnDone(onConnect);

    Context context;
    context.client = std::move(client);
    context.connection = connect(
        this, &NetworkManager::onConnectDone, owner,
        [callback, reqId](
            int requestId, std::shared_ptr<nx::network::http::AsyncClient> client)
        {
            if (reqId == requestId)
                callback(client);
        },
        connectionType);

    // Sending request under mutex to be sure reply is not handled before we store context.
    NX_MUTEX_LOCKER lk(&d->mutex);
    context.client->doConnect(proxyUrl, targetHost);
    d->requests.emplace(reqId, std::move(context));
}

void NetworkManager::doGet(
    std::unique_ptr<nx::network::http::AsyncClient> request,
    nx::utils::Url url,
    QObject* owner,
    RequestCallback callback,
    Qt::ConnectionType connectionType)
{
    doRequest(Method::get, std::move(request), std::move(url), owner, std::move(callback), connectionType);
}

void NetworkManager::doPost(
    std::unique_ptr<nx::network::http::AsyncClient> request,
    nx::utils::Url url,
    QObject* owner,
    RequestCallback callback,
    Qt::ConnectionType connectionType)
{
    doRequest(Method::post, std::move(request), std::move(url), owner, std::move(callback), connectionType);
}

void NetworkManager::doPut(
    std::unique_ptr<nx::network::http::AsyncClient> request,
    nx::utils::Url url,
    QObject* owner,
    RequestCallback callback,
    Qt::ConnectionType connectionType)
{
    doRequest(Method::put, std::move(request), std::move(url), owner, std::move(callback), connectionType);
}

void NetworkManager::doDelete(
    std::unique_ptr<nx::network::http::AsyncClient> request,
    nx::utils::Url url,
    QObject* owner,
    RequestCallback callback,
    Qt::ConnectionType connectionType)
{
    doRequest(Method::delete_, std::move(request), std::move(url), owner, std::move(callback), connectionType);
}

} // namespace nx::vms::client::core
