#include "udp_client.h"

namespace nx {
namespace stun {

const std::chrono::milliseconds UdpClient::kDefaultRetransmissionTimeOut(500);
const int UdpClient::kDefaultMaxRetransmissions = 7;

/********************************************/
/* UdpClient::RequestContext                */
/********************************************/

UdpClient::RequestContext::RequestContext():
    currentRetransmitTimeout(0),
    retryNumber(0)
{
}

UdpClient::RequestContext::RequestContext(RequestContext&& rhs):
    completionHandler(std::move(rhs.completionHandler)),
    currentRetransmitTimeout(std::move(rhs.currentRetransmitTimeout)),
    retryNumber(std::move(rhs.retryNumber)),
    timer(std::move(rhs.timer))
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
}

UdpClient::~UdpClient()
{
    cleanupWhileInAioThread();
}

void UdpClient::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_messagePipeline.pleaseStop(
        [handler = std::move(handler), this]()
        {
            cleanupWhileInAioThread();
            handler();
        });
}

const std::unique_ptr<network::UDPSocket>& UdpClient::socket()
{
    return m_messagePipeline.socket();
}

std::unique_ptr<network::UDPSocket> UdpClient::takeSocket()
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

void UdpClient::messageReceived(SocketAddress sourceAddress, Message message)
{
    auto requestContextIter = m_ongoingRequests.find(message.header.transactionId);
    if (requestContextIter == m_ongoingRequests.end())
    {
        // This may be late response.
        NX_LOGX(lm("Received message from %1 with unexpected transaction id %2")
            .strs(sourceAddress, message.header.transactionId.toHex()), cl_logDEBUG1);
        return;
    }

    if (sourceAddress != requestContextIter->second.resolvedServerAddress)
    {
        NX_LOGX(lm("Received message (transaction id %1) from unexpected address %2")
            .strs(message.header.transactionId.toHex(), sourceAddress), cl_logDEBUG1);
        return;
    }

    auto completionHandler = std::move(requestContextIter->second.completionHandler);
    m_ongoingRequests.erase(requestContextIter);
    message.transportHeader.requestedEndpoint = sourceAddress;   // TODO: #ak
    message.transportHeader.locationEndpoint = sourceAddress;
    completionHandler(SystemError::noError, std::move(message));
}

void UdpClient::ioFailure(SystemError::ErrorCode errorCode)
{
    //TODO #ak ???
    NX_LOGX(lm("I/O error on socket: %1").
        arg(SystemError::toString(errorCode)), cl_logDEBUG1);
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

    // TODO: #ak we should start timer after request has actually been sent.

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

void UdpClient::cleanupWhileInAioThread()
{
    // Timers can be safely removed since we are in aio thread.
    m_ongoingRequests.clear();
}

} // namespace stun
} // namespace nx
