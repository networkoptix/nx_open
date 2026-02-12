// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>

#include "webrtc_datachannel.h"
#include "webrtc_dtls.h"
#include "webrtc_utils.h"

namespace nx::webrtc {

constexpr size_t kIceTcpSendBufferSize = 1024 * 64;

class Ice;

class NX_WEBRTC_API AbstractIceDelegate
{
public:
    // All these methods are called from Ice's AIO Thread.
    virtual bool start(Ice* ice, Dtls* dtls) = 0;
    virtual void stop() = 0;
    virtual bool onSrtp(std::vector<uint8_t> buffer) = 0;
    virtual void onDataChannelString(const std::string& data, int streamId) = 0;
    virtual void onDataChannelBinary(const std::string& data, int streamId) = 0;
    virtual ~AbstractIceDelegate() = default;
};

class Ice:
    public AbstractDataChannelDelegate,
    public AbstractDtlsDelegate
{
protected:
    enum Stage
    {
        binding,
        dtls,
        streaming,
        unknown
    };

public:
    Ice(SessionPool* sessionPool, const SessionConfig& config);
    virtual ~Ice();

    // Main API.
    void writePacket(const char* data, int size, bool foreground);

    // API for DataChannel.
    virtual void writeDataChannelPacket(const uint8_t* data, int size) override final;
    virtual void onDataChannelString(const std::string& data, int streamId) override final;
    virtual void onDataChannelBinary(const std::string& data, int streamId) override final;

    // API for Transceiver.
    void writeBatch(std::deque<nx::Buffer> mediaBuffers);
    void writeDataChannelBatch(const std::deque<nx::Buffer>& mediaBuffers);
    aio::AbstractAioThread* getAioThread();
    void writeDataChannelString(const std::string& data);
    void writeDataChannelBinary(const std::string& data);

    virtual IceCandidate::Filter type() const = 0;

    // API for Dtls.
    virtual void writeDtlsPacket(const char* data, int size) override final;
    virtual bool onDtlsData(const uint8_t* data, int size) override final;
    int bufferEmpty() const;

protected:
    friend class Consumer;

    // API for subclasses.
    virtual nx::Buffer toSendBuffer(const char* data, int dataSize) const;
    virtual void stopUnsafe(); //< Call any time from same AIO thread.
    virtual void asyncSendPacketUnsafe() = 0;
    virtual nx::network::SocketAddress iceRemoteAddress() const = 0;
    virtual nx::network::SocketAddress iceLocalAddress() const = 0;

    void onBytesWritten(SystemError::ErrorCode errorCode, std::size_t bytesTransferred);
    void sendNextBufferUnsafe();
    bool processIncomingPacket(const uint8_t* data, int size);

private:
    // Protocol related.
    Stage demuxMessageStage(const uint8_t* data, size_t size);
    bool handleStun(const uint8_t* data, int size);
    bool handleDtls(const uint8_t* data, int size);
    bool handleSrtp(const uint8_t* data, int size);
    void sendStunError(const stun::Message& message);

protected:
    mutable nx::Mutex m_mutex;
    nx::network::aio::BasicPollable m_pollable;
    bool m_needStop = false;

    std::optional<SessionDescription> m_sessionDescription;

    std::deque<nx::Buffer> m_sendBuffers;
    size_t m_foregroundOffset = 0;
    bool m_hasBufferToSend = false;
    nx::webrtc::SessionPool* m_sessionPool = nullptr;

    Stage m_stage = binding;

private:
    std::weak_ptr<Session> m_session;
};

} // namespace nx::webrtc
