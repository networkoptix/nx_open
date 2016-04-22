/**********************************************************
* Apr 14, 2016
* akolesnikov
***********************************************************/

#include "rendezvous_connector_with_verification.h"

#include <nx/utils/log/log.h>
#include <utils/common/cpp14.h>

#include "nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h"


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
        std::bind(&RendezvousConnectorWithVerification::onConnectCompleted, this, _1, _2));
}

void RendezvousConnectorWithVerification::closeConnection(
    SystemError::ErrorCode closeReason,
    stun::MessagePipeline* connection)
{
    NX_ASSERT(connection == m_requestPipeline.get());

    NX_LOGX(lm("connection %1 error: %2")
        .arg(connectSessionId()).arg(SystemError::toString(closeReason)),
        cl_logDEBUG2);
    m_requestPipeline.reset();
}

void RendezvousConnectorWithVerification::onConnectCompleted(
    SystemError::ErrorCode errorCode,
    std::unique_ptr<UdtStreamSocket> connection)
{
    NX_LOGX(lm("connection %1 completed. result: %2")
        .arg(connectSessionId()).arg(SystemError::toString(errorCode)),
        cl_logDEBUG2);

    if (errorCode != SystemError::noError)
    {
        auto connectCompletionHandler = std::move(m_connectCompletionHandler);
        connectCompletionHandler(errorCode, std::move(connection));
        return;
    }

    //verifying connection id

    connection->bindToAioThread(getAioThread());
    m_requestPipeline = std::make_unique<stun::MessagePipeline>(
        this,
        std::move(connection));
    using namespace std::placeholders;
    m_requestPipeline->setMessageHandler(
        std::bind(&RendezvousConnectorWithVerification::onMessageReceived, this, _1));
    m_requestPipeline->startReadingConnection();

    hpm::api::UdpHolePunchingSyn synRequest;
    stun::Message synRequestMessage(
        nx::stun::Header(
            nx::stun::MessageClass::request,
            stun::cc::methods::udpHolePunchingSyn));
    synRequest.serialize(&synRequestMessage);
    m_requestPipeline->sendMessage(std::move(synRequestMessage));

    if (m_timeout > std::chrono::milliseconds::zero())
        m_aioThreadBinder.start(
            m_timeout,
            std::bind(&RendezvousConnectorWithVerification::onTimeout, this));
}

void RendezvousConnectorWithVerification::onMessageReceived(
    nx::stun::Message message)
{
    if (message.header.messageClass != stun::MessageClass::successResponse)
    {
        NX_LOGX(lm("session %1. Received error instead of syn-ack from %2")
            .arg(connectSessionId()).arg(remoteAddress().toString()),
            cl_logDEBUG1);
        return processError(SystemError::connectionReset);
    }

    hpm::api::UdpHolePunchingSynAck synResponse;
    if (!synResponse.parse(message))
    {
        NX_LOGX(lm("session %1. Error parsing syn-ack from %2: %3")
            .arg(connectSessionId()).arg(remoteAddress().toString())
            .arg(synResponse.errorText()),
            cl_logDEBUG1);
        return processError(SystemError::connectionReset);
    }

    if (synResponse.connectSessionId != connectSessionId())
    {
        NX_LOGX(lm("session %1. Error. Unexpected session id (%2) in syn-ack from %3")
            .arg(connectSessionId()).arg(synResponse.connectSessionId)
            .arg(remoteAddress().toString()),
            cl_logDEBUG1);
        return processError(SystemError::connectionReset);
    }

    NX_LOGX(lm("session %1. Successfully verified connection to %2")
        .arg(connectSessionId()).arg(remoteAddress().toString()),
        cl_logDEBUG2);

    //success!
    auto udtConnection =
        static_unique_ptr_cast<UdtStreamSocket>(m_requestPipeline->takeSocket());
    m_requestPipeline.reset();
    auto connectCompletionHandler = std::move(m_connectCompletionHandler);
    connectCompletionHandler(SystemError::noError, std::move(udtConnection));
}

void RendezvousConnectorWithVerification::onTimeout()
{
    NX_LOGX(lm("connection %1. Error. %2 timeout has expired "
               "while waiting for UdpHolePunchingSynAck")
            .arg(connectSessionId()).arg(m_timeout),
        cl_logDEBUG1);

    processError(SystemError::timedOut);
}

void RendezvousConnectorWithVerification::processError(
    SystemError::ErrorCode errorCode)
{
    m_requestPipeline.reset();
    auto connectCompletionHandler = std::move(m_connectCompletionHandler);
    connectCompletionHandler(errorCode, nullptr);
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
