/**********************************************************
* Dec 29, 2015
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <deque>

#include "message.h"
#include "message_parser.h"
#include "message_serializer.h"
#include "unreliable_message_pipeline.h"


namespace nx {
namespace stun {

/** STUN protocol UDP client.
    Conforms to [rfc5389, 7.2.1]
    \note Supports pipelining
    \note \a UDPClient object can be safely deleted within request completion handler 
        (more generally, within internal socket's aio thread).
        To delete it in another thread, cancel I/O with \a UDPClient::pleaseStop call
 */
class NX_NETWORK_API UDPClient
:
    public QnStoppableAsync,
    private nx::network::UnreliableMessagePipelineEventHandler<Message>
{
public:
    typedef std::function<void(
        SystemError::ErrorCode errorCode,
        Message response)> RequestCompletionHandler;

    static const std::chrono::milliseconds kDefaultRetransmissionTimeOut;
    static const int kDefaultMaxRetransmissions;

    UDPClient();
    UDPClient(SocketAddress serverAddress);

    virtual void pleaseStop(std::function<void()> handler) override;

    /**
        \param request MUST contain unique transactionId
        \param completionHandler \a response is valid only if \a errorCode is 
            \a SystemError::noError. MUST not be NULL
    */
    void sendRequest(
        SocketAddress serverAddress,
        Message request,
        RequestCompletionHandler completionHandler);
    /** This overload can be used if target endpoint has been supplied through constructor */
    void sendRequest(
        Message request,
        RequestCompletionHandler completionHandler);

    const std::unique_ptr<AbstractDatagramSocket>& socket();
    /** If not called, any vacant local port will be used */
    bool bind(const SocketAddress& localAddress);
    SocketAddress localAddress() const;
    /** By default, \a UDPClient::kDefaultRetransmissionTimeOut (500ms).
        Is doubled with each unsuccessful attempt
     */
    void setRetransmissionTimeOut(std::chrono::milliseconds retransmissionTimeOut);
    /** By default, \a UDPClient::kDefaultMaxRetransmissions (7) */
    void setMaxRetransmissions(int maxRetransmissions);

private:
    typedef nx::network::UnreliableMessagePipeline<
        Message,
        MessageParser,
        MessageSerializer> PipelineType;

    class RequestContext
    {
    public:
        RequestCompletionHandler completionHandler;
        std::chrono::milliseconds currentRetransmitTimeout;
        int retryNumber;
        //TODO #ak use some aio thread timer when it is available
        std::unique_ptr<AbstractStreamSocket> timer;
        SocketAddress serverAddress;
        Message request;

        RequestContext();
        RequestContext(RequestContext&&);
    };

    bool m_receivingMessages;
    PipelineType m_messagePipeline;
    std::chrono::milliseconds m_retransmissionTimeout;
    int m_maxRetransmissions;
    //map<transaction id, request data>
    std::map<nx::Buffer, RequestContext> m_ongoingRequests;
    const SocketAddress m_serverAddress;

    virtual void messageReceived(SocketAddress sourceAddress, Message mesage) override;
    virtual void ioFailure(SystemError::ErrorCode) override;

    void sendRequestInternal(
        SocketAddress serverAddress,
        Message request,
        std::function<void(
            SystemError::ErrorCode errorCode,
            Message response)> completionHandler);
    void sendRequestAndStartTimer(
        SocketAddress serverAddress,
        const Message& request,
        RequestContext* const requestContext);
    void timedout(nx::Buffer transactionId);
};

}   //stun
}   //nx
