#pragma once

#include <deque>
#include <string>
#include <type_traits>
#include <vector>

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization/json.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/network/websocket/websocket.h>

#include "discovery_http_api_path.h"

namespace nx {
namespace cloud {
namespace discovery {

enum class ResultCode
{
    ok,
    notFound,
    networkError,
    invalidModuleInformation,
};

enum class PeerStatus
{
    online,
    offline,
};

} // namespace discovery
} // namespace cloud
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::cloud::discovery::ResultCode)(nx::cloud::discovery::PeerStatus),
    (lexical))

namespace nx {
namespace cloud {
namespace discovery {

struct BasicInstanceInformation
{
    std::string type;
    std::string id;
    nx::utils::Url apiUrl;

    BasicInstanceInformation(std::string type):
        type(type)
    {
    }
};

bool operator==(const BasicInstanceInformation& left, const BasicInstanceInformation& right);

#define BasicInstanceInformation_Fields (type)(id)(apiUrl)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (BasicInstanceInformation),
    (json))

//-------------------------------------------------------------------------------------------------

/**
 * @param InstanceInformation Must inherit BasicInstanceInformation
 */
template<typename InstanceInformation>
class ModuleRegistrar:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

    static_assert(
        std::is_base_of<BasicInstanceInformation, InstanceInformation>::value,
        "InstanceInformation MUST be a descendant of BasicInstanceInformation");
    static_assert(
        std::is_same<decltype(InstanceInformation::kTypeName), const char* const>::value,
        "Define static member kTypeName (e.g. relay, mediator, etc...)");

public:
    ModuleRegistrar(
        const nx::utils::Url& baseUrl,
        const std::string& moduleId)
        :
        m_baseUrl(baseUrl),
        m_moduleId(moduleId)
    {
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        if (m_webSocketConnector)
            m_webSocketConnector->bindToAioThread(aioThread);
        if (m_webSocket)
            m_webSocket->bindToAioThread(aioThread);
    }

    void setInstanceInformation(InstanceInformation instanceInformation)
    {
        post(
            [this, instanceInformation = std::move(instanceInformation)]() mutable
            {
                if (!m_webSocket)
                    m_sendQueue.clear();
                else if (m_sendQueue.size() > 1)
                    m_sendQueue.erase(std::next(m_sendQueue.begin()), m_sendQueue.end());

                m_sendQueue.push_back(std::move(instanceInformation));
                if (m_webSocket && m_sendQueue.size() == 1)
                    sendNext();
            });
    }

    void start()
    {
        using namespace std::placeholders;

        auto keepAliveConnectionUrl = m_baseUrl;
        keepAliveConnectionUrl.setPath(nx::network::url::normalizePath(
            keepAliveConnectionUrl.path() + "/" +
            nx::network::http::rest::substituteParameters(
                nx::cloud::discovery::http::kModuleKeepAliveConnectionPath, {m_moduleId}).c_str()));

        m_webSocketConnector = std::make_unique<nx::network::http::AsyncClient>();
        nx::network::websocket::addClientHeaders(m_webSocketConnector.get(), "NxDiscovery");
        m_webSocketConnector->bindToAioThread(getAioThread());
        m_webSocketConnector->doUpgrade(
            keepAliveConnectionUrl,
            nx::network::http::Method::post,
            nx::network::websocket::kWebsocketProtocolName,
            std::bind(&ModuleRegistrar::onWebSocketConnectFinished, this));
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();
        m_webSocketConnector.reset();
        m_webSocket.reset();
    }

private:
    nx::utils::Url m_baseUrl;
    std::unique_ptr<nx::network::http::AsyncClient> m_webSocketConnector;
    std::unique_ptr<nx::network::WebSocket> m_webSocket;
    nx::Buffer m_sendBuffer;
    std::deque<InstanceInformation> m_sendQueue;
    std::string m_moduleId;

    void onWebSocketConnectFinished()
    {
        if (!m_webSocketConnector->hasRequestSucceeded())
        {
            NX_DEBUG(this, lm("Failed to establish websocket connection to %1. "
                "system result code %2, http result code %3")
                .args(
                    m_baseUrl, m_webSocketConnector->lastSysErrorCode(),
                    m_webSocketConnector->response()
                        ? nx::network::http::StatusCode::toString(
                            m_webSocketConnector->response()->statusLine.statusCode)
                        : nx::String("none")));

            // TODO Reconnecting.
            return;
        }

        m_webSocket = std::make_unique<nx::network::WebSocket>(
            m_webSocketConnector->takeSocket(),
            nx::network::websocket::SendMode::singleMessage,
            nx::network::websocket::ReceiveMode::message,
            nx::network::websocket::Role::client);
        m_webSocketConnector.reset();

        if (!m_sendQueue.empty())
            sendNext();
    }

    void sendNext()
    {
        using namespace std::placeholders;

        m_sendBuffer = QJson::serialized(m_sendQueue.front());

        m_webSocket->sendAsync(
            m_sendBuffer,
            std::bind(&ModuleRegistrar::onSent, this, _1, _2));
    }

    void onSent(
        SystemError::ErrorCode systemErrorCode,
        std::size_t bytesSent)
    {
        if (systemErrorCode == SystemError::noError)
        {
            NX_ASSERT(bytesSent == (std::size_t)m_sendBuffer.size());
            m_sendQueue.pop_front();
            if (!m_sendQueue.empty())
                sendNext();
            return;
        }

        if (nx::network::socketCannotRecoverFromError(systemErrorCode))
        {
            // TODO: #ak Reconnecting.
            return;
        }

        // Retrying to send.
        sendNext();
    }
};

//-------------------------------------------------------------------------------------------------

//template<typename T>
//void serialize(QnJsonContext*, const std::vector<T>&, QJsonValue*);

class ModuleFinder:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    ModuleFinder(const nx::utils::Url& baseUrl);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    template<typename InstanceInformation>
    void fetchModules(
        nx::utils::MoveOnlyFunc<void(ResultCode, std::vector<InstanceInformation>)> handler)
    {
        post(
            [this, handler = std::move(handler)]() mutable
            {
                // TODO Include type.
                auto url = m_baseUrl;
                url.setPath(nx::network::url::normalizePath(
                    url.path() + nx::cloud::discovery::http::kDiscoveredModulesPath));

                auto httpClient =
                    std::make_unique<nx::network::http::FusionDataHttpClient<void, std::vector<InstanceInformation>>>(
                        url, nx::network::http::AuthInfo());
                httpClient->bindToAioThread(getAioThread());
                httpClient->execute(
                    [this, handler = std::move(handler), httpClientPtr = httpClient.get()](
                        SystemError::ErrorCode systemErrorCode,
                        const nx::network::http::Response* response,
                        std::vector<InstanceInformation> instanceInformation) mutable
                    {
                        processRequestResult(
                            httpClientPtr,
                            systemErrorCode,
                            response,
                            std::move(instanceInformation),
                            std::move(handler));
                    });
                m_runningRequests.push_back(std::move(httpClient));
            });
    }

protected:
    virtual void stopWhileInAioThread() override;

private:
    const nx::utils::Url m_baseUrl;
    std::vector<std::unique_ptr<nx::network::aio::BasicPollable>> m_runningRequests;

    template<typename InstanceInformation>
    void processRequestResult(
        nx::network::aio::BasicPollable* httpClientPtr,
        SystemError::ErrorCode systemErrorCode,
        const nx::network::http::Response* response,
        std::vector<InstanceInformation> instanceInformation,
        nx::utils::MoveOnlyFunc<void(ResultCode, std::vector<InstanceInformation>)> handler)
    {
        std::unique_ptr<nx::network::aio::BasicPollable> httpClient;
        for (auto it = m_runningRequests.begin(); it != m_runningRequests.end(); ++it)
        {
            if (it->get() != httpClientPtr)
                continue;
            httpClient = std::move(*it);
            m_runningRequests.erase(it);
            break;
        }

        if (systemErrorCode != SystemError::noError)
            return handler(ResultCode::networkError, std::move(instanceInformation));

        if (!(response && nx::network::http::StatusCode::isSuccessCode(response->statusLine.statusCode)))
        {
            // TODO Converting from HTTP status code.
            return handler(ResultCode::networkError, std::move(instanceInformation));
        }

        handler(ResultCode::ok, std::move(instanceInformation));
    }
};

} // namespace discovery
} // namespace cloud
} // namespace nx
