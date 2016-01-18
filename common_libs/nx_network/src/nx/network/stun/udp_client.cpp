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
    m_receivingMessages(false),
    m_messagePipeline(this),
    m_retransmissionTimeout(kDefaultRetransmissionTimeOut),
    m_maxRetransmissions(kDefaultMaxRetransmissions)
{
}

void UDPClient::pleaseStop(std::function<void()> handler)
{
    m_messagePipeline.pleaseStop(
        [/*std::move*/ handler, this](){  //TODO #ak #msvc2015
            //reporting failure for all ongoing requests
            std::vector<RequestCompletionHandler> completionHandlers;
            for (const auto& requestData: m_ongoingRequests)
            {
                completionHandlers.emplace_back(
                    std::move(requestData.second.completionHandler));
            }
            //timers can be safely removed since we are in aio thread
            m_ongoingRequests.clear();
            
            for (auto& completionHandler: completionHandlers)
                completionHandler(
                    SystemError::interrupted,
                    Message());
            handler();
        });
}

const std::unique_ptr<AbstractDatagramSocket>& UDPClient::socket()
{
    return m_messagePipeline.socket();
}

void UDPClient::sendRequest(
    SocketAddress serverAddress,
    Message request,
    RequestCompletionHandler completionHandler)
{
    m_messagePipeline.socket()->dispatch(std::bind(
        &UDPClient::sendRequestInternal,
        this,
        std::move(serverAddress),
        std::move(request),
        std::move(completionHandler)));
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
    assert(insertedValue.second);   //asserting on non-unique transactionID
    RequestContext& requestContext = insertedValue.first->second;
    requestContext.completionHandler = std::move(completionHandler);
    requestContext.currentRetransmitTimeout = m_retransmissionTimeout;
    requestContext.serverAddress = serverAddress;
    requestContext.timer = SocketFactory::createStreamSocket();
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
    m_messagePipeline.sendMessage(
        std::move(serverAddress),
        request,
        std::function<void(SystemError::ErrorCode)>());
    //TODO #ak we should start timer after request has actually been sent

    //starting timer
    requestContext->timer->registerTimer(
        requestContext->currentRetransmitTimeout,
        std::bind(&UDPClient::timedout, this, request.header.transactionId));
}

void UDPClient::timedout(nx::Buffer transactionId)
{
    auto requestContextIter = m_ongoingRequests.find(transactionId);
    assert(requestContextIter != m_ongoingRequests.end());
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
        requestContextIter->second.serverAddress,
        requestContextIter->second.request,
        &requestContextIter->second);
}

}   //stun
}   //nx
