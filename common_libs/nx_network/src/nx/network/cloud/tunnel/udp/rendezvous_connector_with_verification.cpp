#include "rendezvous_connector_with_verification.h"

#include <nx/utils/log/log.h>
#include <nx/utils/type_utils.h>
#include <nx/utils/std/cpp14.h>

#include "nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h"
#include "nx/network/cloud/data/tunnel_connection_chosen_data.h"

namespace nx {
namespace network {
namespace cloud {
namespace udp {

RendezvousConnectorWithVerification::RendezvousConnectorWithVerification(
    nx::String connectSessionId,
    SocketAddress remotePeerAddress,
    std::unique_ptr<nx::network::UDPSocket> udpSocket)
:
    RendezvousConnector(
        std::move(connectSessionId),
        std::move(remotePeerAddress),
        std::move(udpSocket))
{
}

RendezvousConnectorWithVerification::RendezvousConnectorWithVerification(
    nx::String connectSessionId,
    SocketAddress remotePeerAddress,
    SocketAddress localAddressToBindTo)
:
    RendezvousConnector(
        std::move(connectSessionId),
        std::move(remotePeerAddress),
        std::move(localAddressToBindTo))
{
}

RendezvousConnectorWithVerification::~RendezvousConnectorWithVerification()
{
}

void RendezvousConnectorWithVerification::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    RendezvousConnector::pleaseStop(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            m_requestPipeline.reset();
            completionHandler();
        });
}

void RendezvousConnectorWithVerification::connect(
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    m_timeout = timeout;
    m_connectCompletionHandler = std::move(completionHandler);
    RendezvousConnector::connect(
        timeout,
        std::bind(&RendezvousConnectorWithVerification::onConnectCompleted, this, _1));
}

void RendezvousConnectorWithVerification::notifyAboutChoosingConnection(
    ConnectCompletionHandler completionHandler)
{
    NX_ASSERT(m_requestPipeline);
    NX_LOGX(lm("cross-nat %1. Notifying host %2 on connection choice")
        .arg(connectSessionId())
        .arg(m_requestPipeline->socket()->getForeignAddress().toString()),
        cl_logDEBUG2);

    m_connectCompletionHandler = std::move(completionHandler);

    hpm::api::TunnelConnectionChosenRequest tunnelConnectionChosenRequest;
    stun::Message tunnelConnectionChosenMessage;
    tunnelConnectionChosenRequest.serialize(&tunnelConnectionChosenMessage);
    m_requestPipeline->sendMessage(std::move(tunnelConnectionChosenMessage));

    if (m_timeout > std::chrono::milliseconds::zero())
    {
        m_aioThreadBinder.start(
            m_timeout,
            std::bind(
                &RendezvousConnectorWithVerification::onTimeout, this,
                "tunnelConnectionChosenResponse"));
    }
}

std::unique_ptr<nx::network::UdtStreamSocket>
    RendezvousConnectorWithVerification::takeConnection()
{
    auto udtConnection =
        nx::utils::static_unique_ptr_cast<UdtStreamSocket>(
            m_requestPipeline->takeSocket());
    m_requestPipeline.reset();

    return udtConnection;
}

void RendezvousConnectorWithVerification::closeConnection(
    SystemError::ErrorCode closeReason,
    stun::MessagePipeline* connection)
{
    NX_ASSERT(connection == m_requestPipeline.get());

    NX_LOGX(lm("cross-nat %1 error: %2")
        .arg(connectSessionId()).arg(SystemError::toString(closeReason)),
        cl_logDEBUG2);
    m_requestPipeline.reset();
}

void RendezvousConnectorWithVerification::onConnectCompleted(
    SystemError::ErrorCode errorCode)
{
    using namespace std::placeholders;

    std::unique_ptr<UdtStreamSocket> connection = RendezvousConnector::takeConnection();

    NX_LOGX(lm("cross-nat %1 completed. result: %2")
        .arg(connectSessionId()).arg(SystemError::toString(errorCode)),
        cl_logDEBUG2);

    if (errorCode != SystemError::noError)
    {
        nx::utils::swapAndCall(m_connectCompletionHandler, errorCode);
        return;
    }

    // Verifying connection id.

    connection->bindToAioThread(getAioThread());
    m_requestPipeline = std::make_unique<stun::MessagePipeline>(
        this,
        std::move(connection));
    m_requestPipeline->setMessageHandler(
        std::bind(&RendezvousConnectorWithVerification::onMessageReceived, this, _1));
    m_requestPipeline->startReadingConnection();

    hpm::api::UdpHolePunchingSynRequest synRequest;
    stun::Message synRequestMessage;
    synRequest.serialize(&synRequestMessage);
    m_requestPipeline->sendMessage(std::move(synRequestMessage));

    if (m_timeout > std::chrono::milliseconds::zero())
    {
        m_aioThreadBinder.start(
            m_timeout,
            std::bind(
                &RendezvousConnectorWithVerification::onTimeout, this,
                "UdpHolePunchingSynResponse"));
    }
}

void RendezvousConnectorWithVerification::onMessageReceived(
    nx::network::stun::Message message)
{
    if (message.header.messageClass != stun::MessageClass::successResponse)
    {
        NX_LOGX(lm("cross-nat %1. Received error instead of syn-ack from %2")
            .arg(connectSessionId()).arg(remoteAddress().toString()),
            cl_logDEBUG1);
        return processError(SystemError::connectionReset);
    }

    switch (message.header.method)
    {
        case hpm::api::UdpHolePunchingSynResponse::kMethod:
            return processUdpHolePunchingSynAck(std::move(message));

        case hpm::api::TunnelConnectionChosenRequest::kMethod:
            return processTunnelConnectionChosen(std::move(message));

        default:
            NX_LOGX(lm("cross-nat %1. Received unexpected message %2 from %3. Ignoring...")
                .arg(connectSessionId()).arg(message.header.method)
                .arg(remoteAddress().toString()),
                cl_logDEBUG2);
            return;
    }
}

void RendezvousConnectorWithVerification::processUdpHolePunchingSynAck(
    nx::network::stun::Message message)
{
    hpm::api::UdpHolePunchingSynResponse synResponse;
    if (!synResponse.parse(message))
    {
        NX_LOGX(lm("cross-nat %1. Error parsing syn-ack from %2: %3")
            .arg(connectSessionId()).arg(remoteAddress().toString())
            .arg(synResponse.errorText()),
            cl_logDEBUG1);
        return processError(SystemError::connectionReset);
    }

    if (synResponse.connectSessionId != connectSessionId())
    {
        NX_LOGX(lm("cross-nat %1. Error. Unexpected connection id (%2) in syn-ack from %3")
            .arg(connectSessionId()).arg(synResponse.connectSessionId)
            .arg(remoteAddress().toString()),
            cl_logDEBUG1);
        return processError(SystemError::connectionReset);
    }

    NX_LOGX(lm("cross-nat %1. Successfully verified connection to %2")
        .arg(connectSessionId()).arg(remoteAddress().toString()),
        cl_logDEBUG2);

    nx::utils::swapAndCall(m_connectCompletionHandler, SystemError::noError);
}

void RendezvousConnectorWithVerification::processTunnelConnectionChosen(
    nx::network::stun::Message message)
{
    hpm::api::TunnelConnectionChosenResponse responseData;
    if (!responseData.parse(message))
    {
        NX_LOGX(lm("cross-nat %1. Error parsing TunnelConnectionChosenResponse from %2: %3")
            .arg(connectSessionId()).arg(remoteAddress().toString())
            .arg(responseData.errorText()),
            cl_logDEBUG1);
        return processError(SystemError::connectionReset);
    }

    NX_LOGX(lm("cross-nat %1. Successfully notified host %2 about udp tunnel choice")
        .arg(connectSessionId()).arg(remoteAddress().toString()),
        cl_logDEBUG2);

    nx::utils::swapAndCall(m_connectCompletionHandler, SystemError::noError);
}

void RendezvousConnectorWithVerification::onTimeout(nx::String requestName)
{
    NX_LOGX(lm("cross-nat %1. Error. %2 timeout has expired while waiting for %3")
        .arg(connectSessionId()).arg(m_timeout).arg(requestName), cl_logDEBUG1);

    processError(SystemError::timedOut);
}

void RendezvousConnectorWithVerification::processError(
    SystemError::ErrorCode errorCode)
{
    NX_ASSERT(errorCode != SystemError::noError);

    m_requestPipeline.reset();

    nx::utils::swapAndCall(m_connectCompletionHandler, errorCode);
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
