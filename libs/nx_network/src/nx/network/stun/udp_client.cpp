// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "udp_client.h"

#include <nx/utils/string.h>

namespace nx::network::stun {

const std::chrono::milliseconds UdpClient::kDefaultRetransmissionTimeOut(500);
const int UdpClient::kDefaultMaxRetransmissions = 7;
const int UdpClient::kDefaultMaxRedirectCount = 7;

/********************************************/
/* UdpClient::RequestContext                */
/********************************************/

UdpClient::RequestContext::RequestContext():
    currentRetransmitTimeout(0),
    retryNumber(0),
    redirectCount(0)
{
}

/********************************************/
/* UdpClient                                */
/********************************************/

UdpClient::UdpClient():
    UdpClient(SocketAddress())
{
}

UdpClient::UdpClient(SocketAddress serverAddress):
    m_receivingMessages(false),
    m_messagePipeline(this),
    m_retransmissionTimeout(kDefaultRetransmissionTimeOut),
    m_maxRetransmissions(kDefaultMaxRetransmissions),
    m_serverAddress(std::move(serverAddress))
{
    m_messagePipeline.setParserFactory([this]() {
        stun::MessageParser parser;
        parser.setFingerprintRequired(m_fingerprintRequired);
        return parser;
    });

    bindToAioThread(getAioThread());
}

UdpClient::~UdpClient()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void UdpClient::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    BaseType::bindToAioThread(aioThread);

    m_messagePipeline.bindToAioThread(aioThread);
    for (const auto& requestContext: m_ongoingRequests)
        requestContext.second.timer->bindToAioThread(aioThread);
}

const std::unique_ptr<AbstractDatagramSocket>& UdpClient::socket()
{
    return m_messagePipeline.socket();
}

std::unique_ptr<AbstractDatagramSocket> UdpClient::takeSocket()
{
    return m_messagePipeline.takeSocket();
}

void UdpClient::sendRequestTo(
    SocketAddress serverAddress,
    Message request,
    RequestCompletionHandler completionHandler)
{
    m_messagePipeline.socket()->dispatch(
        [this, address = std::move(serverAddress),
               request = std::move(request),
               handler = std::move(completionHandler)]() mutable
        {
            sendRequestInternal(
                std::move(address), std::move(request), std::move(handler));
        });
}

void UdpClient::sendRequest(
    Message request,
    RequestCompletionHandler completionHandler)
{
    sendRequestTo(
        m_serverAddress,
        std::move(request),
        std::move(completionHandler));
}

bool UdpClient::bind(const SocketAddress& localAddress)
{
    return m_messagePipeline.bind(localAddress);
}

SocketAddress UdpClient::localAddress() const
{
    return m_messagePipeline.address();
}

void UdpClient::setRetransmissionTimeOut(
    std::chrono::milliseconds retransmissionTimeOut)
{
    m_retransmissionTimeout = retransmissionTimeOut;
}

void UdpClient::setMaxRetransmissions(int maxRetransmissions)
{
    m_maxRetransmissions = maxRetransmissions;
}

void UdpClient::setFingerprintRequired(bool val)
{
    m_fingerprintRequired = val;
}

void UdpClient::stopWhileInAioThread()
{
    // Timers can be safely removed since we are in aio thread.
    m_ongoingRequests.clear();
    m_messagePipeline.pleaseStopSync();
}

void UdpClient::messageReceived(SocketAddress sourceAddress, Message message)
{
    if (isMessageShouldBeDiscarded(sourceAddress, message))
        return;

    processMessageReceived(std::move(message));
}

void UdpClient::ioFailure(SystemError::ErrorCode errorCode)
{
    //TODO #akolesnikov ???
    NX_DEBUG(this, nx::format("I/O error on socket: %1").
        arg(SystemError::toString(errorCode)));
}

bool UdpClient::isMessageShouldBeDiscarded(
    const SocketAddress& sourceAddress,
    const Message& message)
{
    auto requestContextIter = m_ongoingRequests.find(message.header.transactionId);
    if (requestContextIter == m_ongoingRequests.end())
    {
        // This may be late response.
        NX_DEBUG(this, nx::format("Received message from %1 with unexpected transaction id %2")
            .args(sourceAddress, nx::utils::toHex(message.header.transactionId)));
        return true;
    }

    if (sourceAddress != requestContextIter->second.resolvedServerAddress)
    {
        NX_DEBUG(this, nx::format("Received message (transaction id %1) from unexpected address %2")
            .args(nx::utils::toHex(message.header.transactionId), sourceAddress));
        return true;
    }

    return false;
}

void UdpClient::processMessageReceived(Message message)
{
    if (auto alternateServer = findAlternateServer(message))
    {
        auto requestContextIter = m_ongoingRequests.find(message.header.transactionId);
        NX_ASSERT(requestContextIter != m_ongoingRequests.end());
        if (redirect(&requestContextIter->second, **alternateServer))
            return;
    }

    reportMessage(std::move(message));
}

std::optional<const stun::attrs::AlternateServer*>
    UdpClient::findAlternateServer(const Message& message)
{
    if (message.header.messageClass != MessageClass::errorResponse)
        return std::nullopt;

    const auto* errorCodeAttribute = message.getAttribute<stun::attrs::ErrorCode>();
    if (!errorCodeAttribute)
        return std::nullopt;

    if (errorCodeAttribute->getCode() != error::tryAlternate)
        return std::nullopt;

    const auto* alternateServerAttribute = message.getAttribute<stun::attrs::AlternateServer>();
    if (!alternateServerAttribute)
        return std::nullopt;

    return alternateServerAttribute;
}

bool UdpClient::redirect(
    RequestContext* requestContext,
    const stun::attrs::AlternateServer& where)
{
    if (requestContext->redirectCount >= kDefaultMaxRedirectCount)
        return false;

    ++requestContext->redirectCount;
    requestContext->resolvedServerAddress = where.endpoint();

    sendRequestAndStartTimer(
        where.endpoint(),
        requestContext->request,
        requestContext);

    return true;
}

void UdpClient::reportMessage(Message message)
{
    auto requestContextIter = m_ongoingRequests.find(message.header.transactionId);
    NX_ASSERT(requestContextIter != m_ongoingRequests.end());

    message.transportHeader.requestedEndpoint = requestContextIter->second.originalServerAddress;
    message.transportHeader.locationEndpoint = requestContextIter->second.resolvedServerAddress;

    auto completionHandler = std::move(requestContextIter->second.completionHandler);
    m_ongoingRequests.erase(requestContextIter);
    completionHandler(SystemError::noError, std::move(message));
}

void UdpClient::sendRequestInternal(
    SocketAddress serverAddress,
    Message request,
    RequestCompletionHandler completionHandler)
{
    if (!m_receivingMessages)
    {
        m_messagePipeline.startReceivingMessages();
        m_receivingMessages = true;
    }

    auto insertedValue = m_ongoingRequests.emplace(
        request.header.transactionId, RequestContext());
    NX_ASSERT(insertedValue.second);   //< Asserting on non-unique transactionId.
    RequestContext& requestContext = insertedValue.first->second;
    requestContext.completionHandler = std::move(completionHandler);
    requestContext.currentRetransmitTimeout = m_retransmissionTimeout;
    requestContext.originalServerAddress = serverAddress;
    requestContext.timer = std::make_unique<nx::network::aio::Timer>();
    requestContext.timer->bindToAioThread(m_messagePipeline.socket()->getAioThread());
    requestContext.request = std::move(request);

    sendRequestAndStartTimer(
        std::move(serverAddress),
        requestContext.request,
        &requestContext);
}

void UdpClient::sendRequestAndStartTimer(
    SocketAddress serverAddress,
    const Message& request,
    RequestContext* const requestContext)
{
    auto transactionId = request.header.transactionId;
    m_messagePipeline.sendMessage(
        std::move(serverAddress), request,
        [this, transactionId = std::move(transactionId)](
            SystemError::ErrorCode code, SocketAddress address) mutable
        {
            messageSent(code, std::move(transactionId), std::move(address));
        });

    // TODO: #akolesnikov we should start timer after request has actually been sent.

    requestContext->timer->start(
        requestContext->currentRetransmitTimeout,
        std::bind(&UdpClient::timedOut, this, request.header.transactionId));
}

void UdpClient::messageSent(
    SystemError::ErrorCode errorCode,
    nx::Buffer transactionId,
    SocketAddress resolvedServerAddress)
{
    auto requestContextIter = m_ongoingRequests.find(transactionId);
    if (requestContextIter == m_ongoingRequests.end())
        return; //< Operation may have already timedOut. E.g., network interface is loaded.

    if (errorCode != SystemError::noError)
    {
        ++requestContextIter->second.retryNumber;
        if (requestContextIter->second.retryNumber > m_maxRetransmissions)
        {
            auto completionHandler = std::move(requestContextIter->second.completionHandler);
            m_ongoingRequests.erase(requestContextIter);
            completionHandler(errorCode, Message());
            return;
        }

        //trying once again
        sendRequestAndStartTimer(
            requestContextIter->second.originalServerAddress,
            requestContextIter->second.request,
            &requestContextIter->second);
        return;
    }

    //success
    requestContextIter->second.resolvedServerAddress = std::move(resolvedServerAddress);
}

void UdpClient::timedOut(nx::Buffer transactionId)
{
    auto requestContextIter = m_ongoingRequests.find(transactionId);
    NX_ASSERT(requestContextIter != m_ongoingRequests.end());
    ++requestContextIter->second.retryNumber;
    if (requestContextIter->second.retryNumber > m_maxRetransmissions)
    {
        auto completionHandler = std::move(requestContextIter->second.completionHandler);
        m_ongoingRequests.erase(requestContextIter);
        completionHandler(SystemError::timedOut, Message());
        return;
    }

    requestContextIter->second.currentRetransmitTimeout *= 2;
    sendRequestAndStartTimer(
        requestContextIter->second.originalServerAddress,
        requestContextIter->second.request,
        &requestContextIter->second);
}

} // namespace nx::network::stun
