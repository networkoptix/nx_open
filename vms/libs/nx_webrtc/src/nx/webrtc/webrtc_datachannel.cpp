// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_datachannel.h"

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

#include "webrtc_utils.h"

namespace nx::webrtc {

namespace {

#pragma pack(push, 1)
struct OpenMessage
{
    DataChannel::MessageType type = DataChannel::MessageType::open;
    DataChannel::ChannelType channelType = DataChannel::ChannelType::reliable;
    uint16_t priority;
    uint32_t reliabilityParameter;
    uint16_t labelLength;
    uint16_t protocolLength;
    // The fields with variable length:
    // uint8_t[labelLength] label
    // uint8_t[protocolLength] protocol
};
#pragma pack(pop)

constexpr size_t kDataChannelFragment = 0x4000;

} // namespace

AbstractDataChannelDelegate::AbstractDataChannelDelegate(const SessionConfig& config):
    m_dataChannel(this, kDefaultSctpPort, config)
{
}

AbstractDataChannelDelegate::~AbstractDataChannelDelegate()
{
}

DataChannel::DataChannel(
    AbstractDataChannelDelegate* ice,
    uint16_t sctpPort,
    const SessionConfig& config)
    :
    m_ice(ice),
    m_sctpPort(sctpPort),
    m_config(config)
{
}

DataChannel::~DataChannel()
{
    if (m_sctpSocket)
    {
        usrsctp_shutdown(m_sctpSocket, SHUT_RDWR);
        usrsctp_close(m_sctpSocket);
        m_sctpSocket = nullptr;
        usrsctp_deregister_address(this);
    }
}

void DataChannel::reset()
{
    m_ice = nullptr;
}

bool DataChannel::init()
{
    // There are too many initialization steps, so the initialization is lazy - only when some DTLS
    // packets are actually received for this ICE Candidate.

    // 1. Init the library.
    static nx::utils::ScopeGuard cleanupLibrary(
        []()
        {
            usrsctp_finish();
        });
    static std::once_flag isLibraryInitializated;
    std::call_once(
        isLibraryInitializated,
        []()
        {
            // usrsctp_sysctl...().
            // The send and receive window size of usrsctp is 256KiB, which is too small for
            // realistic RTTs, therefore we increase it to 1MiB by default for better performance.
            // See https://bugzilla.mozilla.org/show_bug.cgi?id=1051685
            usrsctp_sysctl_set_sctp_recvspace(1024 * 1024);
            usrsctp_sysctl_set_sctp_sendspace(1024 * 1024);

            // Increase the maximum number of chunks in the queue to 10K by default.
            usrsctp_sysctl_set_sctp_max_chunks_on_queue(10 * 1024);

            // Increase initial congestion window size to 10 MTUs (RFC 6928) by default
            usrsctp_sysctl_set_sctp_initial_cwnd(10);

            // Set the max burst to 10 MTUs by default
            // (the max burst is initially 0, meaning disabled).
            usrsctp_sysctl_set_sctp_max_burst_default(10);

            // Use the standard SCTP congestion control (RFC 4960) by default.
            // See https://github.com/paullouisageneau/libdatachannel/issues/354
            usrsctp_sysctl_set_sctp_default_cc_module(0);

            // Reduce SACK delay to 20ms by default
            // (the recommended default value from RFC 4960 is 200ms).
            usrsctp_sysctl_set_sctp_delayed_sack_time_default(20);

            // RTO settings.

            // RFC 2988 recommends a 1s min RTO, which is very high,
            // but TCP on Linux has a 200ms min RTO.
            usrsctp_sysctl_set_sctp_rto_min_default(200);
            // Set only 10s as max RTO instead of 60s for shorter connection timeout.
            usrsctp_sysctl_set_sctp_rto_max_default(10'000);
            usrsctp_sysctl_set_sctp_init_rto_max_default(10'000);
            // Set 1s as initial RTO like for min RTO.
            usrsctp_sysctl_set_sctp_rto_initial_default(1'000);

            // RTX settings.

            // Use 5 retransmissions instead of 8 to shorten the backoff for a shorter
            // connection timeout.
            auto maxRtx = 5;
            usrsctp_sysctl_set_sctp_init_rtx_max_default(maxRtx);
            usrsctp_sysctl_set_sctp_assoc_rtx_max_default(maxRtx);
            usrsctp_sysctl_set_sctp_path_rtx_max_default(maxRtx); //< Single path.

            // Heartbeat interval.
            usrsctp_sysctl_set_sctp_heartbeat_interval_default(10000);

            // usrsctp itself. Note that this call will spawn a thread which will wake each usrsctp
            // socket every 10 ms. This is probably safe, because access to socket is atomic.
            usrsctp_init(
                /*port*/ 0,
                &DataChannel::usrsctpWriteCallback,
                &DataChannel::usrsctpDebugCallback);
            usrsctp_enable_crc32c_offload(); //< Computing CRC32 only for outgoing packets.
            usrsctp_sysctl_set_sctp_pr_enable(1); //< Enable Partial Reliability Extension (RFC 3758).
            usrsctp_sysctl_set_sctp_ecn_enable(0); //< Disable Explicit Congestion Notification.
            // Uncomment if you need a verbose debug output with usrsctp internals.
#if 0
            usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);
#endif
        });

    // 2. Init the socket.
    usrsctp_register_address(this);
    m_sctpSocket = usrsctp_socket(
        AF_CONN,
        SOCK_STREAM,
        IPPROTO_SCTP,
        /*receive_cb*/ nullptr,
        /*send_cb*/ nullptr,
        /*sb_threshold*/ 0,
        /*ulp_info*/ nullptr);
    if (!m_sctpSocket)
    {
        NX_DEBUG(this, "Can't create SCTP socket: errno %1", errno);
        return false;
    }
    usrsctp_set_upcall(m_sctpSocket, &DataChannel::usrsctpUpcallCallback, this);

    if (usrsctp_set_non_blocking(m_sctpSocket, /*onoff*/ 1))
    {
        NX_DEBUG(this, "Can't set nonblocking SCTP socket: errno %1", errno);
        return false;
    }

    // SCTP must stop sending after the lower layer is shut down, so disable the linger.
    struct linger socketLinger = {};
    socketLinger.l_onoff = 1;
    socketLinger.l_linger = 0;
    if (usrsctp_setsockopt(
        m_sctpSocket,
        SOL_SOCKET,
        SO_LINGER,
        &socketLinger,
        sizeof(socketLinger)))
    {
        NX_DEBUG(this, "Can't set SO_LINGER on SCTP socket: errno %1", errno);
        return false;
    }

    struct sctp_assoc_value sctpAssocValue = {};
    sctpAssocValue.assoc_id = SCTP_ALL_ASSOC;
    sctpAssocValue.assoc_value = 1;
    if (usrsctp_setsockopt(
        m_sctpSocket,
        IPPROTO_SCTP,
        SCTP_ENABLE_STREAM_RESET,
        &sctpAssocValue,
        sizeof(sctpAssocValue)))
    {
        NX_DEBUG(this, "Can't set SCTP_ENABLE_STREAM_RESET on SCTP socket: errno %1", errno);
        return false;
    }
    int isOn = 1;
    if (usrsctp_setsockopt(m_sctpSocket, IPPROTO_SCTP, SCTP_RECVRCVINFO, &isOn, sizeof(isOn)))
    {
        NX_DEBUG(this, "Can't set SCTP_RECVRCVINFO on SCTP socket: errno %1", errno);
        return false;
    }

    // Set explicit EOR.
    if (usrsctp_setsockopt(m_sctpSocket, IPPROTO_SCTP, SCTP_EXPLICIT_EOR, &isOn, sizeof(isOn)))
    {
        NX_DEBUG(this, "Can't set SCTP_EXPLICIT_EOR on SCTP socket: errno %1", errno);
        return false;
    }

    struct sctp_event sctpEvent = {};
    sctpEvent.se_assoc_id = SCTP_ALL_ASSOC;
    sctpEvent.se_on = 1;
    sctpEvent.se_type = SCTP_ASSOC_CHANGE;
    if (usrsctp_setsockopt(m_sctpSocket, IPPROTO_SCTP, SCTP_EVENT, &sctpEvent, sizeof(sctpEvent)))
    {
        NX_DEBUG(
            this,
            "Can't set subscription to SCTP_ASSOC_CHANGE for SCTP socket: errno %1",
            errno);
        return false;
    }

    sctpEvent.se_type = SCTP_SENDER_DRY_EVENT;
    if (usrsctp_setsockopt(m_sctpSocket, IPPROTO_SCTP, SCTP_EVENT, &sctpEvent, sizeof(sctpEvent)))
    {
        NX_DEBUG(
            this,
            "Can't set subscription to SCTP_SENDER_DRY_EVENT for SCTP socket: errno %1",
            errno);
        return false;
    }
    sctpEvent.se_type = SCTP_STREAM_RESET_EVENT;
    if (usrsctp_setsockopt(m_sctpSocket, IPPROTO_SCTP, SCTP_EVENT, &sctpEvent, sizeof(sctpEvent)))
    {
        NX_DEBUG(
            this,
            "Can't set subscription to SCTP_STREAM_RESET_EVENT for SCTP socket: errno %1",
            errno);
        return false;
    }

    // RFC 8831 6.6. Transferring User Data on a Data Channel
    // The sender SHOULD disable the Nagle algorithm (see [RFC1122) to minimize the latency.
    // See https://www.rfc-editor.org/rfc/rfc8831.html#section-6.6
    int noDelay = 1;
    if (usrsctp_setsockopt(m_sctpSocket, IPPROTO_SCTP, SCTP_NODELAY, &noDelay, sizeof(noDelay)))
    {
        NX_DEBUG(this, "Can't set SCTP_NODELAY for SCTP socket: errno %1", errno);
        return false;
    }

    struct sctp_paddrparams paddrparams = {};
    // Enable SCTP heartbeats
    paddrparams.spp_flags = SPP_HB_ENABLE;

    // RFC 8261 5. DTLS considerations:
    // If path MTU discovery is performed by the SCTP layer and IPv4 is used as the network-layer
    // protocol, the DTLS implementation SHOULD allow the DTLS user to enforce that the
    // corresponding IPv4 packet is sent with the Don't Fragment (DF) bit set. If controlling the DF
    // bit is not possible (for example, due to implementation restrictions), a safe value for the
    // path MTU has to be used by the SCTP stack. It is RECOMMENDED that the safe value not exceed
    // 1200 bytes.
    // See https://www.rfc-editor.org/rfc/rfc8261.html#section-5
    // Fall back to a safe MTU value.
    paddrparams.spp_flags |= SPP_PMTUD_DISABLE;
    // The MTU value provided specifies the space available for chunks in the
    // packet, so we also subtract the SCTP header size.
    constexpr size_t kDefaultSctpMtu =
        /*MTU*/ 1280 - /*SCTP*/ 12 - /*DTLS*/ 48 - /*UDP*/ 8 - /*IPv6*/ 40;
    paddrparams.spp_pathmtu = kDefaultSctpMtu;

    if (usrsctp_setsockopt(
        m_sctpSocket,
        IPPROTO_SCTP,
        SCTP_PEER_ADDR_PARAMS,
        &paddrparams,
        sizeof(paddrparams)))
    {
        NX_DEBUG(this, "Can't set SCTP_PEER_ADDR_PARAMS for SCTP socket: errno %1", errno);
        return false;
    }

    // RFC 8831 6.2. SCTP Association Management
    // The number of streams negotiated during SCTP association setup SHOULD be 65535, which is the
    // maximum number of streams that can be negotiated during the association setup.
    // See https://www.rfc-editor.org/rfc/rfc8831.html#section-6.2
    // However, usrsctp allocates tables to hold the stream states. For 65535 streams, it results in
    // the waste of a few MBs for each association. Therefore, we use a lower limit to save memory.
    // See https://github.com/sctplab/usrsctp/issues/121
    struct sctp_initmsg initMsg = {};
    constexpr int kMaxSctpStreamsCount = 1024;
    initMsg.sinit_num_ostreams = kMaxSctpStreamsCount;
    initMsg.sinit_max_instreams = kMaxSctpStreamsCount;
    if (usrsctp_setsockopt(m_sctpSocket, IPPROTO_SCTP, SCTP_INITMSG, &initMsg, sizeof(initMsg)))
    {
        NX_DEBUG(this, "Can't set SCTP_INITMSG for SCTP socket: errno %1", errno);
        return false;
    }

    // Prevent fragmented interleave of messages (i.e. level 0), see RFC 6458 section 8.1.20.
    // Unless the user has set the fragmentation interleave level to 0, notifications
    // may also be interleaved with partially delivered messages.
    int level = 0;
    if (usrsctp_setsockopt(
        m_sctpSocket,
        IPPROTO_SCTP,
        SCTP_FRAGMENT_INTERLEAVE,
        &level,
        sizeof(level)))
    {
        NX_DEBUG(this, "Can't disable SCTP fragmented interleave: errno %1", errno);
        return false;
    }

    int rcvBuf = 0;
    socklen_t rcvBufLen = sizeof(rcvBuf);
    if (usrsctp_getsockopt(m_sctpSocket, SOL_SOCKET, SO_RCVBUF, &rcvBuf, &rcvBufLen))
    {
        NX_DEBUG(this, "Can't get SCTP recv buffer size for SCTP socket: errno %1", errno);
        return false;
    }

    int sndBuf = 0;
    socklen_t sndBufLen = sizeof(sndBuf);
    if (usrsctp_getsockopt(m_sctpSocket, SOL_SOCKET, SO_SNDBUF, &sndBuf, &sndBufLen))
    {
        NX_DEBUG(this, "Can't get SCTP send buffer size for SCTP socket: errno %1", errno);
        return false;
    }

    // Ensure the buffer is also large enough to accommodate the largest messages.
    constexpr int kMaxMessageSize = 256 * 1024;
    rcvBuf = std::max(rcvBuf, kMaxMessageSize);
    sndBuf = std::max(sndBuf, kMaxMessageSize);

    if (usrsctp_setsockopt(m_sctpSocket, SOL_SOCKET, SO_RCVBUF, &rcvBuf, sizeof(rcvBuf)))
    {
        NX_DEBUG(this, "Can't set SCTP recv buffer size for SCTP socket: errno %1", errno);
        return false;
    }

    if (usrsctp_setsockopt(m_sctpSocket, SOL_SOCKET, SO_SNDBUF, &sndBuf, sizeof(sndBuf)))
    {
        NX_DEBUG(this, "Can't set SCTP send buffer size for SCTP socket: errno %1", errno);
        return false;
    }

    // 3. Connect the socket.
    struct sockaddr_conn sconn = {};
    sconn.sconn_family = AF_CONN;
    sconn.sconn_port = htons(m_sctpPort);
    sconn.sconn_addr = this;
#ifdef HAVE_SCONN_LEN
    sconn.sconn_len = sizeof(sconn);
#endif
    if (usrsctp_bind(m_sctpSocket, (struct sockaddr*) &sconn, sizeof(sconn)))
    {
        NX_DEBUG(this, "Can't bind socket: errno %1", errno);
        return false;
    }
    int ret = usrsctp_connect(m_sctpSocket, (struct sockaddr*) &sconn, sizeof(sconn));
    if (ret && errno != EINPROGRESS)
    {
        NX_DEBUG(this, "Can't connect socket: errno %1, return code %2", errno, ret);
        return false;
    }
    return true;
}

void DataChannel::usrsctpDebugCallback(const char* /*format*/, ...)
{
    // Not implemented.
}

int DataChannel::usrsctpWriteCallback(
    void* ptr,
    void* data,
    size_t len,
    uint8_t /*tos*/,
    uint8_t /*set_df*/)
{
    auto* dataChannel = static_cast<DataChannel*>(ptr);

    if (!dataChannel->m_ice)
        return -1; //< Failure.
    // Set the CRC32 ourselves as we have enabled CRC32 offloading.
    if (len >= 12)
    {
        uint32_t* checksum = (uint32_t*) data + 2;
        *checksum = 0;
        *checksum = usrsctp_crc32c(data, len);
    }
    NX_TRACE(dataChannel, "Write callback called with len %1", len);
    dataChannel->m_ice->writeDataChannelPacket((uint8_t*) data, len);
    return 0; //< Success.
}

void DataChannel::usrsctpUpcallCallback(struct socket*, void* arg, int /*flags*/)
{
    auto* dataChannel = static_cast<DataChannel*>(arg);

    if (!dataChannel->m_ice)
        return;

    NX_CRITICAL(dataChannel->m_sctpSocket);
    dataChannel->m_lastUpcallEvents = usrsctp_get_events(dataChannel->m_sctpSocket);
    NX_TRACE(
        dataChannel,
        "Upcall: %1 (%2%3)",
        dataChannel->m_lastUpcallEvents,
        ((dataChannel->m_lastUpcallEvents & SCTP_EVENT_READ) ? "r" : ""),
        ((dataChannel->m_lastUpcallEvents & SCTP_EVENT_WRITE) ? "w" : ""));
}

bool DataChannel::handleRead()
{
    for (;;)
    {
        if (m_receiveBuffer.size() != kSctpMaxMessageSize)
            m_receiveBuffer.resize(kSctpMaxMessageSize);
        socklen_t fromLen = 0;
        struct sctp_rcvinfo info = {};
        socklen_t infoLen = sizeof(info);
        unsigned int infoType = 0;
        int flags = 0;
        ssize_t len = usrsctp_recvv(m_sctpSocket, m_receiveBuffer.data(), kSctpMaxMessageSize,
            /*from*/ nullptr, &fromLen, &info, &infoLen, &infoType, &flags);
        NX_CRITICAL(len < kSctpMaxMessageSize);

        const auto savedErrno = errno;

        NX_TRACE(this, "SCTP recv: got len %1, errno %2", len, savedErrno);
        if (len < 0)
        {
            if (savedErrno == EWOULDBLOCK || savedErrno == EAGAIN || savedErrno == ECONNRESET)
                return true; //< Not a fatal error.
            else
                return false;
        }

        // SCTP_FRAGMENT_INTERLEAVE does not seem to work as expected for messages > 64KB,
        // therefore partial notifications and messages need to be handled separately.
        if (flags & MSG_NOTIFICATION)
        {
            m_partialNotification.insert(
                m_partialNotification.size(),
                (char*) m_receiveBuffer.data(),
                len);
            if (flags & MSG_EOR)
            {
                // Notification is complete, process it.
                auto notification = (union sctp_notification*) m_partialNotification.data();
                if (!processNotification(notification, m_partialNotification.size()))
                    return false;
                m_partialNotification.clear();
            }
        }
        else
        {
            // SCTP message.
            m_lastMessage.insert(m_lastMessage.size(), (char*) m_receiveBuffer.data(), len);
            if (flags & MSG_EOR)
            {
                // Message is complete, process it.
                if (infoType != SCTP_RECVV_RCVINFO)
                {
                    NX_DEBUG(this, "Missing SCTP recv info");
                    return false;
                }
                bool openedBefore = m_dataChannelOpened;
                bool ret = processReceivedData(
                    (int) info.rcv_sid,
                    (PayloadId) ntohl(info.rcv_ppid));
                m_lastMessage.clear();
                if (!openedBefore && m_dataChannelOpened)
                    return ret && flushSavedMessages();
                return ret;
            }
        }
    }
    return true;
}

int DataChannel::outputQueueSize() const
{
    return m_outputQueue.size();
}

bool DataChannel::handleWrite()
{
    NX_CRITICAL(m_outputQueue.size() > 0);
    auto& message = m_outputQueue.front();
    uint32_t payloadId = 0;
    auto streamId = message.streamId;
    switch (message.type)
    {
        case Message::Type::string:
            payloadId = (message.data.empty() ? PayloadId::stringEmpty : PayloadId::string);
            break;
        case Message::Type::binary:
            payloadId = (message.data.empty() ? PayloadId::binaryEmpty : PayloadId::binary);
            break;
        case Message::Type::control:
            payloadId = PayloadId::control;
            break;
        case Message::Type::reset:
            m_outputQueue.pop();
            return sendStreamReset(streamId);
        default:
            NX_CRITICAL(false);
            break;
    }

    // Always use default reliability parameters: looks like all dataChannel implementations
    // always use TCP-like reliability.

    struct sctp_sendv_spa sendvSpa = {};

    // Set sndinfo.
    sendvSpa.sendv_flags |= SCTP_SEND_SNDINFO_VALID;
    sendvSpa.sendv_sndinfo.snd_sid = uint16_t(message.streamId);
    sendvSpa.sendv_sndinfo.snd_ppid = htonl(payloadId);
    if (message.eor)
        sendvSpa.sendv_sndinfo.snd_flags |= SCTP_EOR;
    else
        sendvSpa.sendv_sndinfo.snd_flags &= ~SCTP_EOR;

    // Set prinfo.
    sendvSpa.sendv_flags |= SCTP_SEND_PRINFO_VALID;

    if (m_config.unreliableTransport)
        sendvSpa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX; //<  // No retransmissions.
    else
        sendvSpa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_NONE; //< // Default reliability setting.
    sendvSpa.sendv_prinfo.pr_value = 0;

    ssize_t ret = 0;
    if (!message.data.empty())
    {
        ret = usrsctp_sendv(
            m_sctpSocket,
            message.data.data(),
            message.data.size(),
            /*to*/ nullptr,
            /*addrcnt*/ 0,
            &sendvSpa,
            sizeof(sendvSpa),
            SCTP_SENDV_SPA,
            /*flags*/ 0);
    }
    else
    {
        const char zero = 0;
        ret = usrsctp_sendv(
            m_sctpSocket,
            /*data*/ &zero,
            /*size*/ 1,
            /*to*/ nullptr,
            /*addrcnt*/ 0,
            &sendvSpa,
            sizeof(sendvSpa),
            SCTP_SENDV_SPA,
            /*flags*/ 0);
    }

    if (ret < 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            // This case is unexpected, but not an error.
            NX_DEBUG(this, "SCTP sending not available");
            return true;
        }
        else
        {
            NX_DEBUG(this, "SCTP sending failed: errno %1", errno);
            return false;
        }
    }
    NX_TRACE(this, "SCTP send size = %1 ret = %2", message.data.size(), ret);
    if (message.data.size() == (size_t) ret)
    {
        m_outputQueue.pop();
    }
    else
    {
        NX_ASSERT((size_t) ret < message.data.size());
        message.data.erase(0, ret);
    }
    return true;
}

bool DataChannel::sendStreamReset(int streamId)
{
    const size_t len = sizeof(struct sctp_reset_streams) + sizeof(uint16_t);
    uint8_t buffer[len] = {};
    struct sctp_reset_streams& resetStreams = *(struct sctp_reset_streams*) buffer;
    resetStreams.srs_flags = SCTP_STREAM_RESET_OUTGOING;
    resetStreams.srs_number_streams = 1;
    resetStreams.srs_stream_list[0] = streamId;

    if (usrsctp_setsockopt(m_sctpSocket, IPPROTO_SCTP, SCTP_RESET_STREAMS, &resetStreams, len) == 0)
        return true;
    if (errno == EINVAL)
    {
        NX_VERBOSE(this, "SCTP stream %1 already reset", streamId);
        return true; //< Not a fatal error.
    }
    else
    {
        NX_VERBOSE(this, "SCTP reset stream %1 failed, errno %2", streamId, errno);
        return false;
    }
}

bool DataChannel::handleIo()
{
    NX_CRITICAL(m_sctpSocket);

    bool handled = false;
    do
    {
        handled = false;
        if (m_lastUpcallEvents & SCTP_EVENT_READ)
        {
            handled = true;
            // Upcall callback can be called with new event set on usrsctp_recvv.
            m_lastUpcallEvents &= ~SCTP_EVENT_READ;
            if (!handleRead())
                return false;
        }
        if ((m_lastUpcallEvents & SCTP_EVENT_WRITE) && m_outputQueue.size() > 0)
        {
            handled = true;
            // Upcall callback can be called with new event set on usrsctp_sendv.
            m_lastUpcallEvents &= ~SCTP_EVENT_WRITE;
            if (!handleWrite())
                return false;
        }
        // Loop until the internal usrsctp event queue is empty, and there is no data to send
        // in the outputQueue.
    } while (handled);
    return true;
}

bool DataChannel::closeStream(int streamId)
{
    if (!m_dataChannelOpened)
        return true;
    if (streamId < 0)
        streamId = defaultStream();
    m_dataChannelOpened = false;
    // RFC 8831 6.7. Closing a Data Channel
    // Closing of a data channel MUST be signaled by resetting the corresponding outgoing streams.
    // See https://www.rfc-editor.org/rfc/rfc8831.html#section-6.7
    return writeMessage({Message::Type::reset, streamId, std::string(), true});
}

bool DataChannel::writeString(const std::string& data, int streamId)
{
    if (streamId < 0)
        streamId = defaultStream();
    return writeUserMessage({Message::Type::string, streamId, data, true});
}

bool DataChannel::writeBinary(const std::string& data, int streamId)
{
    if (streamId < 0)
        streamId = defaultStream();
    return writeUserMessage({Message::Type::binary, streamId, data, true});
}

bool DataChannel::flushSavedMessages()
{
    NX_ASSERT(m_dataChannelOpened);
    while (!m_savedQueue.empty())
    {
        if (!writeMessage(m_savedQueue.front()))
            return false;
        m_savedQueue.pop();
    }
    return true;
}

bool DataChannel::writeUserMessage(Message&& message)
{
    if (!m_dataChannelOpened)
    {
        constexpr int kSavedQueueLimit = 64;

        if (m_savedQueue.size() >= kSavedQueueLimit)
        {
            NX_DEBUG(this, "Saved queue size overflow: %1", m_savedQueue.size());
            return false;
        }
        NX_TRACE(this, "Saved message with type %1", (int) message.type);
        m_savedQueue.emplace(std::move(message));
        return true;
    }
    else
    {
        return writeMessage(message);
    }
}

bool DataChannel::writeMessage(Message message)
{
    if (!m_sctpSocket && !init())
        return false;

    constexpr int kOutputQueueLimit = 1024 * 4; //< Can grow due to fragmentation.
    if (m_outputQueue.size() >= kOutputQueueLimit)
    {
        NX_DEBUG(this, "Output queue size overflow: %1", m_outputQueue.size());
        return false;
    }
    NX_VERBOSE(this, "Added message with type %1 size %2", (int) message.type, message.data.size());
    m_lastUpcallEvents |= SCTP_EVENT_WRITE;
    if (message.data.size() <= kDataChannelFragment)
    {
        m_outputQueue.emplace(std::move(message));
        return handleIo();
    }

    size_t offset = 0;
    while (offset != message.data.size())
    {
        // usrsctp don't support a large messages (>~200k) due to its internal buffer limitations.
        // So we need to fragment message first.
        size_t len = std::min(kDataChannelFragment, message.data.size() - offset);
        // Fragment original message.
        Message fragmentedMessage
        {
            message.type,
            message.streamId,
            {message.data.data() + offset, len},
            len + offset == message.data.size() ? message.eor : false
        };
        m_outputQueue.emplace(std::move(fragmentedMessage));
        offset += len;
    }
    return handleIo();
}

bool DataChannel::handlePacket(const uint8_t* data, int size)
{
    if (!m_sctpSocket && !init())
        return false;

    // So strange API. Add some data to socket, then recv from this socket.
    usrsctp_conninput(this, data, size, /*ecn_bits*/ 0);

    return handleIo();
}

bool DataChannel::processReceivedData(int streamId, PayloadId payloadId)
{
    // RFC 8831: The usage of the PPIDs "WebRTC String Partial" and "WebRTC Binary Partial" is
    // deprecated. They were used for a PPID-based fragmentation and reassembly of user messages
    // belonging to reliable and ordered data channels.
    // See https://www.rfc-editor.org/rfc/rfc8831.html#section-6.6.
    // We handle those PPIDs at reception for compatibility reasons but shall never send them.
    switch (payloadId)
    {
        case control:
        {
            NX_VERBOSE(this, "Got control, type = %1", (int) m_lastMessage[0]);
            return processReceivedMessage(
                {
                    Message::Type::control,
                    streamId,
                    m_lastMessage
                });
        }

        case stringPart: //< Deprecated.
        {
            m_partialString.insert(m_partialString.size(), m_lastMessage);
            break;
        }

        case string:
        {
            if (m_partialString.empty())
            {
                return processReceivedMessage(
                    {
                        Message::Type::string,
                        streamId,
                        m_lastMessage
                    });
            }
            else
            {
                std::string tempstr(m_partialString);
                tempstr.insert(tempstr.size(), m_lastMessage);
                m_partialString.clear();
                return processReceivedMessage(
                    {
                        Message::Type::string,
                        streamId,
                        tempstr,
                        true
                    });
            }
            break;
        }

        case stringEmpty:
        {
            // Don't report into the dataChannel level.
            m_partialString.clear();
            break;
        }

        case binaryPart: //< Deprecated.
        {
            m_partialBinary.insert(m_partialBinary.size(), m_lastMessage);
            break;
        }

        case binary:
        {
            if (m_partialBinary.empty())
            {
                return processReceivedMessage(
                    {
                        Message::Type::binary,
                        streamId,
                        m_lastMessage,
                        true
                    });
            }
            else
            {
                std::string tempbin(m_partialBinary);
                tempbin.insert(tempbin.size(), m_lastMessage);
                m_partialBinary.clear();
                return processReceivedMessage(
                    {
                        Message::Type::binary,
                        streamId,
                        tempbin,
                        true
                    });
            }
            break;
        }

        case binaryEmpty:
        {
            // Don't report into the dataChannel level.
            m_partialBinary.clear();
            break;
        }

        default:
        {
            // Unknown
            NX_DEBUG(this, "Unknown PPID: %1", (uint32_t) payloadId);
            break;
        }
    }
    return true;
}

bool DataChannel::processNotification(const union sctp_notification* notification, int len)
{
    if (len < (int) sizeof(struct sctp_notification::sctp_tlv)
        || len != (int) notification->sn_header.sn_length)
    {
        NX_DEBUG(this, "Got bad notification with length %1", len);
        return true; //< Not a fatal error.
    }

    auto type = notification->sn_header.sn_type;
    NX_VERBOSE(this, "Processing notification, type: %1", type);

    switch (type)
    {
        case SCTP_ASSOC_CHANGE:
        {
            NX_VERBOSE(this, "SCTP association change event");
            const struct sctp_assoc_change& assocChange = notification->sn_assoc_change;
            if (assocChange.sac_state == SCTP_COMM_UP)
            {
                NX_VERBOSE(this, "SCTP negotiated streams: incoming = %1, outgoing = %2",
                    assocChange.sac_inbound_streams, assocChange.sac_outbound_streams);
                // Not used at this moment.
                m_negotiatedStreamCount.emplace(
                    std::min(assocChange.sac_inbound_streams, assocChange.sac_outbound_streams));
                return true;
            }
            else
            {
                NX_DEBUG(this, "SCTP negotiation failed or closed");
                return false;
            }
            break;
        }

        case SCTP_SENDER_DRY_EVENT:
        {
            NX_TRACE(this, "SCTP sender dry event");
            // Do nothing, write() will be already called.
            break;
        }

        case SCTP_STREAM_RESET_EVENT:
        {
            const struct sctp_stream_reset_event& resetEvent = notification->sn_strreset_event;
            const int count = (resetEvent.strreset_length - sizeof(resetEvent)) / sizeof(uint16_t);
            const uint16_t flags = resetEvent.strreset_flags;

            {
                std::ostringstream description;
                description << "flags=";
                if (
                    (flags & SCTP_STREAM_RESET_OUTGOING_SSN && flags)
                    & SCTP_STREAM_RESET_INCOMING_SSN)
                {
                    description << "outgoing|incoming";
                }
                else if (flags & SCTP_STREAM_RESET_OUTGOING_SSN)
                {
                    description << "outgoing";
                }
                else if (flags & SCTP_STREAM_RESET_INCOMING_SSN)
                {
                    description << "incoming";
                }
                else
                {
                    description << "0";
                }

                description << ", streams=[";
                for (int i = 0; i < count; ++i)
                {
                    uint16_t streamId = resetEvent.strreset_stream_list[i];
                    description << (i != 0 ? "," : "") << streamId;
                }
                description << "]";

                NX_VERBOSE(this, "SCTP reset event: %1", description.str());
            }

            // RFC 8831 6.7. Closing a Data Channel.
            // If one side decides to close the data channel, it resets the corresponding outgoing
            // stream. When the peer sees that an incoming stream was reset, it also resets its
            // corresponding outgoing stream.
            // See https://www.rfc-editor.org/rfc/rfc8831.html#section-6.7
            if (flags & SCTP_STREAM_RESET_INCOMING_SSN)
            {
                const char dataChannelCloseMessage{(char) MessageType::close};
                for (int i = 0; i < count; ++i)
                {
                    uint16_t streamId = resetEvent.strreset_stream_list[i];
                    // Pass reset as control with `close` message into dataChannel level.
                    return processReceivedMessage(
                        {
                            Message::Type::control,
                            streamId,
                            std::string(&dataChannelCloseMessage, &dataChannelCloseMessage + 1),
                            true
                        });
                }
            }
            break;
        }

        default:
            // Ignore.
            break;
    }
    return true;
}

bool DataChannel::processReceivedMessage(const Message& message)
{
    switch (message.type)
    {
        case Message::Type::control:
        {
            if (message.data.empty())
                return true; //< Ignore.
            // It's a DCEP level.
            uint8_t raw = (uint8_t) message.data[0];
            switch (raw)
            {
                case MessageType::open:
                    return processOpenMessage(message);
                case MessageType::ack:
                    m_dataChannelOpened = true;
                    NX_DEBUG(
                        this,
                        "Opened local Data Channel with stream %1, label %2, protocol %3",
                        m_defaultStreamId,
                        m_label,
                        m_protocol);
                    break;
                case MessageType::close:
                    m_dataChannelOpened = false;
                    break;
                default:
                    break;
            }
            break;
        }
        case Message::Type::reset:
            // Unused type, `reset` is handled as `control`.
            break;
        case Message::Type::string:
            NX_ASSERT(!message.data.empty()); //< Unexpected at dataChannel level.
            if (m_ice)
                m_ice->onDataChannelString(message.data, message.streamId);
            break;
        case Message::Type::binary:
            NX_ASSERT(!message.data.empty()); //< Unexpected at dataChannel level.
            if (m_ice)
                m_ice->onDataChannelBinary(message.data, message.streamId);
            break;
    }
    return true;
}

bool DataChannel::processOpenMessage(const Message& message)
{
    if (message.data.size() < sizeof(OpenMessage))
    {
        NX_DEBUG(this,
            "Invalid open message size: %1 vs %2",
            message.data.size(),
            sizeof(OpenMessage));
        return false;
    }

    OpenMessage openMessage = *(const OpenMessage*) message.data.data();
    openMessage.priority = ntohs(openMessage.priority);
    openMessage.reliabilityParameter = ntohl(openMessage.reliabilityParameter);
    openMessage.labelLength = ntohs(openMessage.labelLength);
    openMessage.protocolLength = ntohs(openMessage.protocolLength);

    if (message.data.size() <
        sizeof(OpenMessage) + (size_t) (openMessage.labelLength + openMessage.protocolLength))
    {
        NX_DEBUG(this, "Truncated open message size: %1 vs %2",
            message.data.size(),
            sizeof(OpenMessage) + (size_t) (openMessage.labelLength + openMessage.protocolLength));
        return false;
    }

    auto end = (const char*) (message.data.data() + sizeof(OpenMessage));
    m_label.assign(end, openMessage.labelLength);
    m_protocol.assign(end + openMessage.labelLength, openMessage.protocolLength);

    // We don't parse reliability parameter: looks like all dataChannel implementations
    // always use TCP-like reliability.

    if (m_dataChannelOpened)
    {
        NX_DEBUG(
            this,
            "Data channel %1 is already open, but requested another channel %2"
            "to open from remote. Will read from both opened channels, "
            "but write only into first opened channel.",
            m_defaultStreamId,
            message.streamId);
    }
    else
    {
        m_defaultStreamId = message.streamId;
        m_dataChannelOpened = true;
    }

    NX_DEBUG(this, "Opened remote Data Channel with stream %1, label %2, protocol %3",
        message.streamId, m_label, m_protocol);

    uint8_t ack = MessageType::ack;
    return writeMessage(
        {Message::Type::control, message.streamId, std::string((const char*) &ack, 1), true});
}

bool DataChannel::openDataChannel(const std::string& label, const std::string& protocol)
{
    if (m_dataChannelOpened)
        return true; //< Don't need another Data Channel.
    m_label = label;
    m_protocol = protocol;

    // Always use default reliability parameters: looks like all dataChannel implementations
    // always use TCP-like reliability.

    ChannelType channelType = ChannelType::reliable;
    uint32_t reliabilityParameter = 0;

    const size_t len = sizeof(OpenMessage) + m_label.size() + m_protocol.size();
    std::string buffer(len, 0);
    auto& openMessage = *(OpenMessage*) buffer.data();
    openMessage.type = MessageType::open;
    openMessage.channelType = channelType;
    openMessage.priority = htons(0);
    openMessage.reliabilityParameter = htonl(reliabilityParameter);
    openMessage.labelLength = htons((uint16_t) m_label.size());
    openMessage.protocolLength = htons((uint16_t) m_protocol.size());

    const auto end = buffer.data() + sizeof(OpenMessage);
    std::copy(m_label.begin(), m_label.end(), end);
    std::copy(m_protocol.begin(), m_protocol.end(), end + m_label.size());

    return writeMessage({Message::Type::control, m_defaultStreamId, buffer, true});
}

} // namespace nx::webrtc
