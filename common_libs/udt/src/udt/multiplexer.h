#pragma once

#include <memory>

class CSndQueue;
class CRcvQueue;
class CChannel;
class CTimer;

/**
 * Manages reading/writing an UDP socket. A single UDP socket can be used by multiple UDT sockets.
 */
class Multiplexer
{
public:
    Multiplexer(
        int ipVersion,
        int payloadSize);
    virtual ~Multiplexer();

    void start();

    // The UDP channel for sending and receiving
    std::shared_ptr<CChannel> m_pChannel;
    std::shared_ptr<CTimer> m_pTimer;
    std::shared_ptr<CSndQueue> m_pSndQueue;
    std::shared_ptr<CRcvQueue> m_pRcvQueue;

    // The UDP port number of this multiplexer
    int m_iPort = 0;
    int m_iIPversion = 0;
    // Maximum Segment Size
    int m_iMSS = 0;
    // number of UDT instances that are associated with this multiplexer
    int m_iRefCount = 0;
    // if this one can be shared with others
    bool m_bReusable = 0;

    int m_iID = 0;

    void shutdown();
};
