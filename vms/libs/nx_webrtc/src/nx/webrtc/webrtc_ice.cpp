// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_ice.h"

#include "webrtc_session.h"
#include "webrtc_session_pool.h"

namespace nx::webrtc {

using namespace nx::vms::api;
using namespace nx::network;
using namespace nx;

Ice::Ice(SessionPool* sessionPool, const SessionConfig& config):
    AbstractDataChannelDelegate(config),
    m_sessionPool(sessionPool)
{
}

/*virtual*/ Ice::~Ice()
{
}

/*virtual*/ nx::Buffer Ice::toSendBuffer(const char* data, int dataSize) const
{
    return nx::Buffer(data, dataSize);
}

void Ice::sendStunError(const stun::Message& message)
{
    stun::Message response;
    response.header.messageClass = stun::MessageClass::errorResponse;
    response.header.method = message.header.method;
    response.header.transactionId = message.header.transactionId;

    stun::MessageSerializer stunSerializer;
    nx::Buffer sendBuffer = stunSerializer.serialized(response);

    writePacket(sendBuffer.data(), sendBuffer.size(), /*foreground*/ true);
}

bool Ice::handleStun(const uint8_t* data, int size)
{
    // 1. Parse
    nx::Buffer buffer((const char*)data, size);
    stun::Message message;
    stun::MessageParser stunParser;
    stunParser.setFingerprintRequired(true);
    stunParser.setMessage(&message);
    size_t parsed = 0;
    if (stunParser.parse(buffer, &parsed) != nx::network::server::ParserState::done)
    {
        NX_VERBOSE(this, "STUN message not parsed");
        return true; //< Not a fatal error.
    }

    // 2. Check
    if ((message.header.messageClass != stun::MessageClass::request
        && message.header.messageClass != stun::MessageClass::successResponse)
        || message.header.method != stun::MethodType::bindingMethod)
    {
        NX_VERBOSE(this, "STUN binding message wrong header");
        return true; //< Not a fatal error.
    }
    auto iceControlling = message.getAttribute<stun::attrs::IceControlling>();
    auto iceControlled = message.getAttribute<stun::attrs::IceControlled>();
    auto useCandidate = message.getAttribute<stun::attrs::UseCandidate>();

    // Next 2 checks are not exactly valid due to client role is unimplemented.
    if (!!iceControlling == !!iceControlled)
    {
        NX_VERBOSE(this, "STUN unexpected 'ice-controlling' and 'ice-controlled': %1 vs %2",
            !!iceControlling, !!iceControlled);
    }

    bool controlling = (iceControlling == nullptr);
    if (controlling && !!useCandidate)
    {
        NX_VERBOSE(this, "STUN unexpected 'controlling' and 'use-candidate': %1 vs %2",
            controlling, !!useCandidate);
    }
    auto priority = message.getAttribute<stun::attrs::Priority>();
    if (!priority && message.header.messageClass == stun::MessageClass::request)
    {
        NX_VERBOSE(this, "STUN no PRIORITY attribute");
        sendStunError(message);
        return true; //< Not a fatal error.
    }
    // TODO Provide additional checks: priority should match with priority from SDP.

    // USERNAME from STUN is in format "<local ice-ufrag>:<remote ice-ufrag>"
    auto username = message.getAttribute<stun::attrs::UserName>();
    if (!username)
    {
        NX_VERBOSE(this, "STUN no USERNAME attribute");
        sendStunError(message);
        return false;
    }

    std::string stunUsername = username->getString();
    auto colonPos = stunUsername.find(":");
    if (colonPos == std::string::npos)
    {
        NX_VERBOSE(this, "STUN wrong USERNAME attribute format");
        sendStunError(message);
        return false;
    }
    auto leftUsername = stunUsername.substr(0, colonPos);
    auto rightUsername = stunUsername.substr(colonPos + 1);

    if (!m_sessionDescription)
    {
        m_sessionDescription = m_sessionPool->describe(leftUsername);
        if (!m_sessionDescription)
        {
            NX_DEBUG(this, "STUN session %1 is locked or expired", leftUsername);
            sendStunError(message);
            return false;
        }
    }
    else if (m_sessionDescription->id != leftUsername)
    {
        NX_DEBUG(this, "STUN heartbeat with mismatch USERNAME: %1 vs %2", m_sessionDescription->id, leftUsername);
        sendStunError(message);
        return false;
    }
    if (m_sessionDescription->remoteUfrag != rightUsername)
    {
        NX_DEBUG(this, "STUN session: right username mismatch: %1 vs %2",
            rightUsername,
            m_sessionDescription->remoteUfrag);
        sendStunError(message);
        return false;
    }

    if (!message.verifyIntegrity(stunUsername, m_sessionDescription->localPwd))
    {
        NX_DEBUG(this, "STUN integrity check failed with user %1", stunUsername);
        sendStunError(message);
        return false;
    }

    if (message.header.messageClass == stun::MessageClass::successResponse)
    {
        NX_VERBOSE(this, "STUN got success response after binding %1", m_sessionDescription->id);
        if (m_stage == Stage::binding)
            m_stage = Stage::dtls;
        return true;
    }

    // 3. Write response
    stun::Message response;
    response.header.messageClass = stun::MessageClass::successResponse;
    response.header.method = stun::MethodType::bindingMethod;
    response.header.transactionId = message.header.transactionId;

    if (iceRemoteAddress().address.ipV4())
        response.newAttribute<stun::attrs::XorMappedAddress>(
            iceRemoteAddress().port, ntohl(iceRemoteAddress().address.ipV4()->s_addr));

    if (controlling)
        response.newAttribute<stun::attrs::UseCandidate>();

    response.newAttribute<stun::attrs::Realm>(nx::Buffer("nx"));
    response.insertIntegrity(stunUsername, m_sessionDescription->localPwd);

    stun::MessageSerializer stunSerializer;
    stunSerializer.setAlwaysAddFingerprint(true);
    nx::Buffer sendBuffer = stunSerializer.serialized(response);

    writePacket(sendBuffer.data(), sendBuffer.size(), /*foreground*/ true);

    NX_DEBUG(this, "STUN handshake successful for session %1", m_sessionDescription->id);

    // 4. Change stage
    if (m_stage == Stage::binding)
        m_stage = Stage::dtls;

    return true;
}

/*virtual*/ void Ice::writeDataChannelPacket(const uint8_t* data, int size)
{
    nx::Buffer buffer((const char*) data, size);

    m_pollable.dispatch([this, buffer = std::move(buffer)]()
        {
            if (m_stage == Stage::streaming && m_sessionDescription)
            {
                // TODO probably mutex in DTLS can be removed.
                if (Dtls::Status::error == m_sessionDescription->dtls->writePacket(
                    (const uint8_t*) buffer.data(), buffer.size(), this))
                {
                    NX_DEBUG(
                        this,
                        "%1: Failed to write datachannel packet",
                        m_sessionDescription->id);
                }
            }
        });
}

/*virtual*/ void Ice::writeDtlsPacket(const char* data, int size)
{
    writePacket(data, size, /*foreground*/ true);
}

/*virtual*/ bool Ice::onDtlsData(const uint8_t* data, int size)
{
    return m_dataChannel.handlePacket(data, size);
}

bool Ice::handleDtls(const uint8_t* data, int size)
{
    NX_CRITICAL(m_sessionDescription);
    auto status = m_sessionDescription->dtls->handlePacket(data, size, this);
    if (status == Dtls::Status::error)
        return false;
    if (status == Dtls::Status::streaming && m_stage == Stage::dtls)
    {
        m_stage = Stage::streaming;

        /* Some peers (at least Firefox) make successfully STUN handshake into several ICE candidates
         * in parallel, and then choose one of them to make DTLS handshake.
         * This one chosen with remote peer can mismatch with one who first locked session
         * on the server side.
         * So, lock session afterward, on successful DTLS handshake.
         * */
        NX_CRITICAL(!m_session);
        m_session = m_sessionPool->lock(m_sessionDescription->id);

        if (!m_session)
        {
            NX_DEBUG(this, "Init DTLS: can't lock session %1", m_sessionDescription->id);
            return false;
        }

        if (!startStream())
            return false;
    }
    return true;
}

bool Ice::handleSrtp(const uint8_t* data, int size)
{
    if (!m_session)
        return true; //< Not a fatal error.

    std::vector<uint8_t> buffer;
    buffer.assign(data, data + size);

    return m_session->transceiver()->onSrtp(std::move(buffer));
}

Ice::Stage Ice::demuxMessageStage(const uint8_t* data, size_t size)
{
    NX_CRITICAL(size >= 2);
    if (((data[0] == 0x0 || data[0] == 0x1) && data[1] == 0x1) || m_stage == Stage::binding)
    {
        /* Actually, second byte 0x1 means binding request.
         * Not all STUN requests begins from 0x0 0x1.
         * Also, binding request is also used as heartbeat in streaming stage */
        return Stage::binding;
    }
    else if (data[1] == 0xfe || m_stage == Stage::dtls)
    {
        return Stage::dtls;
    }
    else if (m_stage == Stage::streaming)
    {
        /* Assume it's RTP/RTCP */
        return Stage::streaming;
    }
    else
    {
        return Stage::unknown;
    }
}

bool Ice::processIncomingPacket(const uint8_t* data, int size)
{
    Stage currentStage;
    if (size < 2 || (currentStage = demuxMessageStage(data, size)) == Stage::unknown)
    {
        NX_DEBUG(
            this,
            "Unexpected packet received for %1",
            (m_sessionDescription ? m_sessionDescription->id : "(null)"));
        return true; //< Not a fatal error.
    }

    switch (currentStage)
    {
        case Stage::binding:
            return handleStun(data, size);
        case Stage::dtls:
            return handleDtls(data, size);
        case Stage::streaming:
            return handleSrtp(data, size);
        default:
            NX_ASSERT(false);
            return false;
    }
}

void Ice::writePacket(const char* data, int size, bool foreground)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (foreground && m_foregroundOffset < m_sendBuffers.size())
        m_sendBuffers.insert(m_sendBuffers.begin() + m_foregroundOffset, toSendBuffer(data, size));
    else
        m_sendBuffers.push_back(toSendBuffer(data, size));
    if (foreground)
        ++m_foregroundOffset;

    sendNextBufferUnsafe();
}

void Ice::writeBatch(const std::deque<nx::Buffer>& mediaBuffers)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    // Always non-foreground.
    for (const auto& packet: mediaBuffers)
        m_sendBuffers.push_back(toSendBuffer(packet.data(), packet.size()));
    sendNextBufferUnsafe();
}

void Ice::writeDataChannelBatch(const std::deque<nx::Buffer>& mediaBuffers)
{
    for (const auto& packet: mediaBuffers)
        writeDataChannelBinary({packet.data(), packet.size()});
}

aio::AbstractAioThread* Ice::getAioThread()
{
    return m_pollable.getAioThread();
}

bool Ice::startStream()
{
    if (m_needStop)
        return false;

    m_dataChannel.openDataChannel("nx-channel", "");

    if (!m_session->transceiver()->start(this, m_sessionDescription->dtls.get()))
        return false;

    return true;
}

void Ice::onBytesWritten(SystemError::ErrorCode errorCode, std::size_t /*bytesTransferred*/)
{
    if (m_needStop)
        return;

    if (nx::network::socketCannotRecoverFromError(errorCode))
    {
        NX_DEBUG(
            this,
            "%1: got socket write error: %2",
            (m_sessionDescription ? m_sessionDescription->id : "(null)"),
            SystemError::toString(errorCode));
        stopUnsafe();
        return;
    }

    NX_MUTEX_LOCKER lock(&m_mutex);

    m_sendBuffers.pop_front();
    if (m_foregroundOffset != 0)
        --m_foregroundOffset;

    m_hasBufferToSend = false;
    sendNextBufferUnsafe();
}

void Ice::sendNextBufferUnsafe()
{
    if (m_hasBufferToSend)
        return;

    if (m_needStop)
        return;

    if (m_sendBuffers.empty())
    {
        if (m_session && m_session->consumer() && m_dataChannel.outputQueueSize() == 0)
            m_session->consumer()->onDataSent(/*success*/ true);

        return;
    }

    m_hasBufferToSend = true;
    asyncSendPacketUnsafe();
}

void Ice::stopUnsafe()
{
    m_needStop = true;
    m_dataChannel.reset();
    m_pollable.pleaseStopSync();
    if (m_session)
    {
        if (m_session->consumer())
            m_session->consumer()->stopUnsafe();
        m_session->sessionPool()->unlock(m_session->getLocalUfrag());
        m_session.reset();
    }
}

/*virtual*/ void Ice::onDataChannelString(const std::string& data, int streamId)
{
    NX_DEBUG(this, "Got string from Data Channel: %1", data);
    m_session->transceiver()->onDataChannelString(data, streamId);
}

/*virtual*/ void Ice::onDataChannelBinary(const std::string& data, int streamId)
{
    NX_DEBUG(this, "Got binary from Data Channel: %1", data);
    m_session->transceiver()->onDataChannelBinary(data, streamId);
}

void Ice::writeDataChannelString(const std::string& data)
{
    m_pollable.dispatch(
        [this, data]()
        {
            if (!m_dataChannel.writeString(data))
                stopUnsafe();
        });
}

void Ice::writeDataChannelBinary(const std::string& data)
{
    m_pollable.dispatch(
        [this, data]()
        {
            if (!m_dataChannel.writeBinary(data))
                stopUnsafe();
        });
}

int Ice::bufferEmpty() const
{
    return m_sendBuffers.empty() && m_dataChannel.outputQueueSize() == 0;
}

} // namespace nx::webrtc
