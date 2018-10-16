#include <string>
#include <functional>

#include <nx/network/websocket/websocket.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/std/future.h>
#include <nx/utils/move_only_func.h>

using namespace nx::network;

enum class Role
{
    server,
    client
};

struct Config
{
    Role role;
    std::string protocolName;
};

WebSocketPtr establishConnection(const std::string& url)
{
}

void startListening(const SocketAddress& address,
    nx::utils::MoveOnlyFunc<void(WebSocketPtr)> onAccept)
{
}

int main(int argc, const char *argv[])
{

    //nx::utils::promise<void> readyPromise;
    //auto readyFuture = readyPromise.get_future();

    //http::AsyncClient httpClient;
    //http::HttpHeaders headers;
    //websocket::addClientHeaders(&headers, "nxp2p");
    //httpClient.setAdditionalHeaders(headers);

    //httpClient.doGet("http://admin:admin@127.0.0.1:7001/ec2/transactionBus",
    //    [&readyPromise, &httpClient]()
    //    {
    //        if (httpClient.state() == nx::network::http::AsyncClient::State::sFailed
    //            || !httpClient.response())
    //        {
    //            std::cout << "http client failed" << std::endl;
    //            readyPromise.set_value();
    //            return;
    //        }

    //        const int statusCode = m_httpClient->response()->statusLine.statusCode;

    //        NX_VERBOSE(this, lit("%1. statusCode = %2").arg(Q_FUNC_INFO).arg(statusCode));

    //        if (statusCode == nx::network::http::StatusCode::unauthorized)
    //        {
    //            // try next credential source
    //            m_credentialsSource = (CredentialsSource)((int)m_credentialsSource + 1);
    //            if (m_credentialsSource < CredentialsSource::none)
    //            {
    //                using namespace std::placeholders;
    //                fillAuthInfo(m_httpClient.get(), m_credentialsSource == CredentialsSource::serverKey);
    //                m_httpClient->doGet(
    //                    m_httpClient->url(),
    //                    std::bind(&ConnectionBase::onHttpClientDone, this));
    //            }
    //            else
    //            {
    //                cancelConnecting(State::Unauthorized, lm("Unauthorized"));
    //            }
    //            return;
    //        }
    //        else if (!nx::network::http::StatusCode::isSuccessCode(statusCode)) //< Checking that statusCode is 2xx.
    //        {
    //            cancelConnecting(State::Error, lm("Not success HTTP status code %1").arg(statusCode));
    //            return;
    //        }
    //    });

    //websocket::WebSocket webSocket;

    //readyPromise.wait();
}