/**********************************************************
* Dec 29, 2015
* akolesnikov
***********************************************************/

#include "udp_client.h"


namespace nx {
namespace stun {

const std::chrono::milliseconds UDPClient::kDefaultRetransmissionTimeOut(500);
const int UDPClient::kDefaultMaxRetransmissions = 7;

/********************************************/
/* UDPClient::RequestContext                */
/********************************************/

UDPClient::RequestContext::RequestContext()
:
    currentRetransmitTimeout(0),
    retryNumber(0)
{
}

UDPClient::RequestContext::RequestContext(RequestContext&& rhs)
:
    completionHandler(std::move(rhs.completionHandler)),
    currentRetransmitTimeout(std::move(rhs.currentRetransmitTimeout)),
    retryNumber(std::move(rhs.retryNumber)),
    timer(std::move(rhs.timer))
{
}

/********************************************/
/* UDPClient                                */
/********************************************/

UDPClient::UDPClient()
:
    UDPClient(SocketAddress())
{
}

UDPClient::UDPClient(SocketAddress serverAddress)
:
    m_receivingMessages(false),
    m_messagePipeline(this),
    m_retransmissionTimeout(kDefaultRetransmissionTimeOut),
    m_maxRetransmissions(kDefaultMaxRetransmissions),
    m_serverAddress(std::move(serverAddress))
{
}

UDPClient::~UDPClient()
{
    //if not in aio thread and pleaseStop has not been called earlier - 
        //undefined behavior can occur
    cleanupWhileInAioThread();
}

void UDPClient::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_messagePipeline.pleaseStop(
        [handler = std::move(handler), this](){
            cleanupWhileInAioThread();
            handler();
        });
}

const std::unique_ptr<network::UDPSocket>& UDPClient::socket()
{
    return m_messagePipeline.socket();
}

std::unique_ptr<network::UDPSocket> UDPClient::takeSocket()
{
    return m_messagePipeline.takeSocket();
}

void UDPClient::sendRequestTo(
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

void UDPClient::sendRequest(
    Message request,
    RequestCompletionHandler completionHandler)
{
    sendRequestTo(
        m_serverAddress,
        std::move(request),
        std::move(completionHandler));
}

bool UDPClient::bind(const SocketAddress& localAddress)
{
    return m_messagePipeline.bind(localAddress);
}

SocketAddress UDPClient::localAddress() const
{
    return m_messagePipeline.address();
}

void UDPClient::setRetransmissionTimeOut(
    std::chrono::milliseconds retransmissionTimeOut)
{
    m_retransmissionTimeout = retransmissionTimeOut;
}

void UDPClient::setMaxRetransmissions(int maxRetransmissions)
{
    m_maxRetransmissions = maxRetransmissions;
}

void UDPClient::messageReceived(SocketAddress sourceAddress, Message message)
{
    auto requestContextIter = m_ongoingRequests.find(message.header.transactionId);
    if (requestContextIter == m_ongoingRequests.end())
    {
        //this may be late response
        NX_LOGX(lm("Received message with from %1 with unexpected transaction id %2").
            arg(sourceAddress.toString()).arg(message.header.transactionId),
            cl_logDEBUG1);
        return;
    }

    if (sourceAddress != requestContextIter->second.resolvedServerAddress)
    {
        NX_LOGX(lm("Received message (transaction id %1) from unexpected address %2").
            arg(message.header.transactionId).arg(sourceAddress.toString()),
            cl_logDEBUG1);
        return;
    }

    auto completionHandler = std::move(requestContextIter->second.completionHandler);
    m_ongoingRequests.erase(requestContextIter);
    completionHandler(SystemError::noError, std::move(message));
}

void UDPClient::ioFailure(SystemError::ErrorCode errorCode)
{
    //TODO #ak ???
    NX_LOGX(lm("I/O error on socket: %1").
        arg(SystemError::toString(errorCode)), cl_logDEBUG1);
}

void UDPClient::sendRequestInternal(
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
    NX_ASSERT(insertedValue.second);   //asserting on non-unique transactionID
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

void UDPClient::sendRequestAndStartTimer(
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

    //TODO #ak we should start timer after request has actually been sent

    //starting timer
    requestContext->timer->start(
        requestContext->currentRetransmitTimeout,
        std::bind(&UDPClient::timedOut, this, request.header.transactionId));
}

void UDPClient::messageSent(
    SystemError::ErrorCode errorCode,
    nx::Buffer transactionId,
    SocketAddress resolvedServerAddress)
{
    auto requestContextIter = m_ongoingRequests.find(transactionId);
    if (requestContextIter == m_ongoingRequests.end())
        return; //operation may have already timedOut. E.g., network interface is loaded

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

void UDPClient::timedOut(nx::Buffer transactionId)
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

void UDPClient::cleanupWhileInAioThread()
{
    //reporting failure for all ongoing requests
    std::vector<RequestCompletionHandler> completionHandlers;
    for (auto& requestData : m_ongoingRequests)
    {
        completionHandlers.push_back(
            std::move(requestData.second.completionHandler));
    }
    //timers can be safely removed since we are in aio thread
    m_ongoingRequests.clear();

    for (auto& completionHandler : completionHandlers)
        completionHandler(
            SystemError::interrupted,
            Message());
}

}   //stun
}   //nx
