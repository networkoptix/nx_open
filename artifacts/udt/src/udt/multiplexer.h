#pragma once

#include <memory>

class CSndQueue;
class CRcvQueue;
class AbstractUdpChannel;
class CTimer;

/**
 * Manages reading/writing an UDP socket. A single UDP socket can be used by multiple UDT sockets.
 */
class UDT_API Multiplexer
{
public:
    Multiplexer(
        int ipVersion,
        int payloadSize,
        int maximumSegmentSize,
        bool reusable,
        int id);
    virtual ~Multiplexer();

    void start();

    void shutdown();

    AbstractUdpChannel& channel();
    CSndQueue& sendQueue();
    CRcvQueue& recvQueue();

    /** @return local UDP port the multiplexer uses. */
    int udpPort() const;

    const int ipVersion;
    const int maximumSegmentSize;
    // if this one can be shared with others
    const bool reusable;
    const int id;

private:
    std::unique_ptr<AbstractUdpChannel> m_udpChannel;
    std::unique_ptr<CTimer> m_timer;
    std::unique_ptr<CSndQueue> m_sendQueue;
    std::unique_ptr<CRcvQueue> m_recvQueue;
};
