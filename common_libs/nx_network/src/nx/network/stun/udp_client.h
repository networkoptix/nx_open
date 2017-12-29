#pragma once

#include <chrono>
#include <deque>

#include <boost/optional.hpp>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>

#include "message.h"
#include "message_parser.h"
#include "message_serializer.h"
#include "unreliable_message_pipeline.h"

namespace nx {
namespace network {
namespace stun {

typedef nx::network::UnreliableMessagePipeline<
    Message,
    MessageParser,
    MessageSerializer> UnreliableMessagePipeline;
typedef nx::network::UnreliableMessagePipelineEventHandler<Message>
    UnreliableMessagePipelineEventHandler;

/**
 * STUN protocol UDP client.
 * Conforms to [rfc5389, 7.2.1]
 * NOTE: Supports pipelining
 * NOTE: UdpClient object can be safely deleted within request completion handler
 *     (more generally, within internal socket's aio thread).
       To delete it in another thread, cancel I/O with UdpClient::pleaseStop call
 * NOTE: Notifies everyone who is waiting for response by reporting
 *     SystemError::interrupted before destruction.
 */
class NX_NETWORK_API UdpClient:
    public network::aio::BasicPollable,
    private nx::network::UnreliableMessagePipelineEventHandler<Message>
{
    using BaseType = network::aio::BasicPollable;

public:
    typedef utils::MoveOnlyFunc<void(
        SystemError::ErrorCode errorCode,
        Message response)> RequestCompletionHandler;

    static const std::chrono::milliseconds kDefaultRetransmissionTimeOut;
    static const int kDefaultMaxRetransmissions;
    static const int kDefaultMaxRedirectCount;

    UdpClient();
    UdpClient(SocketAddress serverAddress);
    virtual ~UdpClient() override;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    /**
     * @param request MUST contain unique transactionId
     * @param completionHandler response is valid only if errorCode is
     *     SystemError::noError. MUST not be NULL
     */
    void sendRequestTo(
        SocketAddress serverAddress,
        Message request,
        RequestCompletionHandler completionHandler);
    /**
     * This overload can be used if target endpoint has been supplied through constructor.
     */
    void sendRequest(
        Message request,
        RequestCompletionHandler completionHandler);

    const std::unique_ptr<network::UDPSocket>& socket();
    /**
     * Move ownership of socket to the caller.
     * UdpClient is in undefined state after this call and MUST be freed
     * NOTE: Can be called within send/recv completion handler
     *     (more specifically, within socket's aio thread) only!
     */
    std::unique_ptr<network::UDPSocket> takeSocket();
    /**
     * If not called, any vacant local port will be used.
     */
    bool bind(const SocketAddress& localAddress);
    SocketAddress localAddress() const;
    /**
     * By default, UdpClient::kDefaultRetransmissionTimeOut (500ms).
     * Is doubled with each unsuccessful attempt
     */
    void setRetransmissionTimeOut(std::chrono::milliseconds retransmissionTimeOut);
    /**
     * By default, UdpClient::kDefaultMaxRetransmissions (7).
     */
    void setMaxRetransmissions(int maxRetransmissions);

private:
    typedef UnreliableMessagePipeline PipelineType;

    class RequestContext
    {
    public:
        RequestCompletionHandler completionHandler;
        std::chrono::milliseconds currentRetransmitTimeout;
        int retryNumber;
        std::unique_ptr<nx::network::aio::Timer> timer;
        SocketAddress originalServerAddress;
        /**
         * This address reported by socket on send completion.
         */
        SocketAddress resolvedServerAddress;
        Message request;
        int redirectCount;

        RequestContext();

        RequestContext(RequestContext&&) = default;
        RequestContext& operator=(RequestContext&&) = default;
    };

    bool m_receivingMessages;
    PipelineType m_messagePipeline;
    std::chrono::milliseconds m_retransmissionTimeout;
    int m_maxRetransmissions;
    //map<transaction id, request data>
    std::map<nx::Buffer, RequestContext> m_ongoingRequests;
    const SocketAddress m_serverAddress;

    virtual void stopWhileInAioThread() override;

    virtual void messageReceived(SocketAddress sourceAddress, Message message) override;
    virtual void ioFailure(SystemError::ErrorCode) override;

    bool isMessageShouldBeDiscarded(
        const SocketAddress& sourceAddress,
        const Message& message);
    void processMessageReceived(Message message);
    boost::optional<const stun::attrs::AlternateServer*>
        findAlternateServer(const Message& mesage);
    bool redirect(RequestContext* requestContext, const stun::attrs::AlternateServer& where);
    void reportMessage(Message message);

    void sendRequestInternal(
        SocketAddress serverAddress,
        Message request,
        RequestCompletionHandler completionHandler);
    void sendRequestAndStartTimer(
        SocketAddress serverAddress,
        const Message& request,
        RequestContext* const requestContext);
    void messageSent(
        SystemError::ErrorCode errorCode,
        nx::Buffer transactionId,
        SocketAddress resolvedServerAddress);
    void timedOut(nx::Buffer transactionId);
};

} // namespace stun
} // namespace network
} // namespace nx
