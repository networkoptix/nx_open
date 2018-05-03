#include "mediaserver_endpoint_tester.h"

#include <QJsonDocument>

#include <nx/utils/log/log.h>

#include <api/model/ping_reply.h>
#include <rest/server/json_rest_result.h>

namespace nx {
namespace hpm {

struct PingCollector
{
    QnMutex mutex;
    size_t expected;
    std::list<network::SocketAddress> endpoints;
    ConnectionWeakRef connection;

    PingCollector(
        size_t expected,
        ConnectionWeakRef connection)
        :
        expected(expected),
        connection(std::move(connection))
    {
    }
};

//-------------------------------------------------------------------------------------------------

MediaserverEndpointTesterBase::MediaserverEndpointTesterBase(
    AbstractCloudDataProvider* cloudData)
    :
    RequestProcessor(cloudData)
{
}

void MediaserverEndpointTesterBase::ping(
    const ConnectionStrongRef& connection,
    network::stun::Message message)
{
    MediaserverData mediaserverData;
    nx::String errorMessage;
    const api::ResultCode resultCode =
        getMediaserverData(*connection, message, &mediaserverData, &errorMessage);
    if (resultCode != api::ResultCode::ok)
    {
        sendErrorResponse(
            connection,
            message.header,
            resultCode,
            api::resultCodeToStunErrorCode(resultCode),
            errorMessage);
        return;
    }

    const auto endpointsAttr =
        message.getAttribute<network::stun::extension::attrs::PublicEndpointList>();
    if (!endpointsAttr)
    {
        sendErrorResponse(
            connection,
            message.header,
            api::ResultCode::badRequest,
            network::stun::error::badRequest,
            "Attribute PublicEndpointList is required");
        return;
    }

    const auto endpoints = endpointsAttr->get();
    const auto collector = std::make_shared< PingCollector >(endpoints.size(), connection);
    const auto& method = message.header.method;
    const auto& transactionId = message.header.transactionId;
    const auto onPinged = [=](network::SocketAddress address, bool result)
    {
        QnMutexLocker lk(&collector->mutex);
        if (result)
            collector->endpoints.push_back(std::move(address));

        if (--collector->expected)
            return; //< Waiting for others to complete.

        if (auto connection = collector->connection.lock())
        {
            NX_LOGX(lit("Peer %1.%2 succesfully pinged %3")
                .arg(QString::fromUtf8(mediaserverData.systemId))
                .arg(QString::fromUtf8(mediaserverData.serverId))
                .arg(containerString(collector->endpoints)), cl_logDEBUG1);

            network::stun::Message response(network::stun::Header(
                network::stun::MessageClass::successResponse, method,
                std::move(transactionId)));

            response.newAttribute< network::stun::extension::attrs::PublicEndpointList >(
                collector->endpoints);

            connection->sendMessage(std::move(response));
        }
    };

    for (auto& ep: endpoints)
        pingServer(ep, mediaserverData.serverId, onPinged);
}

//-------------------------------------------------------------------------------------------------

MediaserverEndpointTester::MediaserverEndpointTester(
    AbstractCloudDataProvider* cloudData)
    :
    MediaserverEndpointTesterBase(cloudData)
{
}

static QString scanResponseForErrors(const nx::Buffer& buffer, const String& expectedId)
{
    QnJsonRestResult result;
    if (!QJson::deserialize(buffer, &result))
        return lit("bad REST result: %3").arg(QString::fromUtf8(buffer));

    QnPingReply reply;
    if (!QJson::deserialize(result.reply, &reply))
        return lit("bad REST reply: %3").arg(result.reply.toString());

    if (reply.moduleGuid.toSimpleString().toUtf8() != expectedId)
        return lit("moduleGuid '%3' doesnt match expectedId '%4'")
        .arg(reply.moduleGuid.toString())
        .arg(QString::fromUtf8(expectedId));

    return QString();
}

void MediaserverEndpointTester::pingServer(
    const network::SocketAddress& address,
    const String& expectedId,
    std::function<void(network::SocketAddress, bool)> onPinged)
{
    auto httpClient = nx::network::http::AsyncHttpClient::create();
    QObject::connect(
        httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        [=](nx::network::http::AsyncHttpClientPtr httpClient)
        {
            {
                QnMutexLocker lk(&m_mutex);
                m_httpClients.erase(httpClient);
            }

            if (!httpClient->hasRequestSucceeded())
            {
                NX_LOGX(lit("Response from %1 has failed")
                    .arg(address.toString()), cl_logDEBUG1);
                return onPinged(std::move(address), false);
            }

            const auto buffer = httpClient->fetchMessageBodyBuffer();
            const auto error = scanResponseForErrors(buffer, expectedId);
            if (!error.isEmpty())
            {
                NX_LOGX(lit("Response from %1 has error: %2")
                    .arg(address.toString()).arg(error), cl_logDEBUG1);
                return onPinged(std::move(address), false);
            }
            onPinged(std::move(address), true);
        });

    httpClient->doGet(lit("http://%1/api/ping").arg(address.toString()));

    QnMutexLocker lk(&m_mutex);
    m_httpClients.insert(std::move(httpClient));
}

} // namespace hpm
} // namespace nx
