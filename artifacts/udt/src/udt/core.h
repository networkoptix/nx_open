/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.

* Redistributions in binary form must reproduce the
above copyright notice, this list of conditions
and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
nor the names of its contributors may be used to
endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
Yunhong Gu, last updated 02/28/2012
*****************************************************************************/

#ifndef __UDT_CORE_H__
#define __UDT_CORE_H__

#include <memory>
#include <mutex>

#include "udt.h"
#include "common.h"
#include "list.h"
#include "connection_buffer.h"
#include "window.h"
#include "packet.h"
#include "channel.h"
#include "api.h"
#include "ccc.h"
#include "cache.h"
#include "result.h"
#include "queue.h"

class Multiplexer;

/**
 * Processes handshake requests.
 */
class ServerSideConnectionAcceptor
{
public:
    ServerSideConnectionAcceptor(
        std::chrono::microseconds startTime,
        UDTSockType sockType,
        UDTSOCKET socketId,
        CSndQueue* sndQueue,
        std::set<int> pollIds);

    /**
     * If packet does not contain connection request, non-zero is returned.
     */
    int processConnectionRequest(const detail::SocketAddress& addr, const CPacket& packet);

    void addEPoll(const int eid);
    void removeEPoll(const int eid);

private:
    bool m_closing = false;
    std::chrono::microseconds m_StartTime = std::chrono::microseconds::zero();
    UDTSockType m_iSockType;
    UDTSOCKET m_SocketId;
    CSndQueue* m_sendQueue = nullptr;
    std::set<int> m_pollIds;
};

//-------------------------------------------------------------------------------------------------

enum class ConnectState
{
    inProgress,
    done,
};

class UDT_API CUDT:
    public std::enable_shared_from_this<CUDT>
{
public: //API
    CUDT();
    CUDT(const CUDT& ancestor);
    CUDT& operator=(const CUDT&) = delete;
    ~CUDT();

    static Result<> startup();
    static Result<> cleanup();
    static Result<UDTSOCKET> socket(int af, int type = SOCK_STREAM, int protocol = 0);
    static Result<> bind(UDTSOCKET u, const sockaddr* name, int namelen);
    static Result<> bind(UDTSOCKET u, UDPSOCKET udpsock);
    static Result<> listen(UDTSOCKET u, int backlog);
    static Result<UDTSOCKET> accept(UDTSOCKET u, sockaddr* addr, int* addrlen);
    static Result<> connect(UDTSOCKET u, const sockaddr* name, int namelen);
    static Result<> shutdown(UDTSOCKET u, int how);
    static Result<> close(UDTSOCKET u);
    static Result<> getpeername(UDTSOCKET u, sockaddr* name, int* namelen);
    static Result<> getsockname(UDTSOCKET u, sockaddr* name, int* namelen);
    static Result<> getsockopt(UDTSOCKET u, int level, UDTOpt optname, void* optval, int* optlen);
    static Result<> setsockopt(UDTSOCKET u, int level, UDTOpt optname, const void* optval, int optlen);
    static Result<int> send(UDTSOCKET u, const char* buf, int len, int flags);
    static Result<int> recv(UDTSOCKET u, char* buf, int len, int flags);
    static Result<int> sendmsg(UDTSOCKET u, const char* buf, int len, int ttl = -1, bool inorder = false);
    static Result<int> recvmsg(UDTSOCKET u, char* buf, int len);
    static Result<int64_t> sendfile(UDTSOCKET u, std::fstream& ifs, int64_t& offset, int64_t size, int block = 364000);
    static Result<int64_t> recvfile(UDTSOCKET u, std::fstream& ofs, int64_t& offset, int64_t size, int block = 7280000);

    static Result<int> select(
        int nfds,
        ud_set* readfds, ud_set* writefds, ud_set* exceptfds,
        const timeval* timeout);

    static Result<int> selectEx(
        const std::vector<UDTSOCKET>& fds,
        std::vector<UDTSOCKET>* readfds,
        std::vector<UDTSOCKET>* writefds,
        std::vector<UDTSOCKET>* exceptfds,
        int64_t msTimeOut);

    static Result<int> epoll_create();
    static Result<void> epoll_add_usock(const int eid, const UDTSOCKET u, const int* events = NULL);
    static Result<void> epoll_add_ssock(const int eid, const SYSSOCKET s, const int* events = NULL);
    static Result<void> epoll_remove_usock(const int eid, const UDTSOCKET u);
    static Result<void> epoll_remove_ssock(const int eid, const SYSSOCKET s);

    static Result<int> epoll_wait(
        const int eid,
        std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds, int64_t msTimeOut,
        std::map<SYSSOCKET, int>* lrfds = NULL, std::map<SYSSOCKET, int>* wrfds = NULL);

    static Result<void> epoll_interrupt_wait(const int eid);
    static Result<void> epoll_release(const int eid);
    static const Error& getlasterror();
    static Result<> perfmon(UDTSOCKET u, CPerfMon* perf, bool clear = true);
    static UDTSTATUS getsockstate(UDTSOCKET u);

public: // internal API
    static Result<std::shared_ptr<CUDT>> getUDTHandle(UDTSOCKET u);

    void setConnecting(bool val);
    bool isConnecting() const;

    bool connected() const { return m_bConnected; }
    bool listening() const { return m_bListening; }
    bool shutdown() const { return m_bShutdown; }

    int decrementBrokenCounter() { return m_iBrokenCounter--; }

    void setIsClosing(bool val);
    bool isClosing() const;

    void setBroken(bool val);
    bool broken() const;

    int payloadSize() const { return m_iPayloadSize; }

    std::chrono::microseconds lastReqTime() const { return m_llLastReqTime; }
    void setLastReqTime(std::chrono::microseconds value) { m_llLastReqTime = value; }

    void addEPoll(const int eid, int eventsToReport);
    void removeEPoll(const int eid);
    const std::set<int>& pollIds() const { return m_sPollID; }

    bool rendezvous() const { return m_bRendezvous; }

    const CHandShake& connReq() const { return m_ConnReq; }
    const CHandShake& connRes() const { return m_ConnRes; }

    UDTSOCKET socketId() const { return m_SocketId; }
    UDTSockType sockType() const { return m_iSockType; }
    int ipVersion() const { return m_iIPversion; }

    void setSocket(UDTSOCKET socketId, UDTSockType sockType, int ipVersion)
    {
        m_SocketId = socketId;
        m_iSockType = sockType;
        m_iIPversion = ipVersion;
    }

    UDTSOCKET peerId() const { return m_PeerID; }

    CSndQueue& sndQueue();
    CRcvQueue& rcvQueue();

    void setMultiplexer(const std::shared_ptr<Multiplexer>& multiplexer);

    SendBuffer* sndBuffer() { return m_pSndBuffer.get(); }
    ReceiveBuffer* rcvBuffer() { return m_pRcvBuffer.get(); }

    bool synRecving() const { return m_bSynRecving; }
    bool synSending() const { return m_bSynSending; }
    detail::SocketAddress& peerAddr() { return m_pPeerAddr; };
    const uint32_t* selfIp() const { return m_piSelfIP; }

    int32_t isn() const { return m_iISN; }
    int mss() const { return m_iMSS; }
    int flightFlagSize() const { return m_iFlightFlagSize; }
    int sndBufSize() const { return m_iSndBufSize; }
    int rcvBufSize() const { return m_iRcvBufSize; }

    struct linger linger() const { return m_Linger; }
    int udpSndBufSize() const { return m_iUDPSndBufSize; }
    int udpRcvBufSize() const { return m_iUDPRcvBufSize; }

    std::chrono::microseconds lingerExpiration() const { return m_ullLingerExpiration; }
    void setLingerExpiration(std::chrono::microseconds value) { m_ullLingerExpiration = value; }

    bool reuseAddr() const { return m_bReuseAddr; }

    void setCache(CCache<CInfoBlock>* value) { m_pCache = value; }

    // Functionality:
    //    initialize a UDT entity and bind to a local address.
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    void open();

    // Functionality:
    //    Start listening to any connection request.
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    Result<> listen();

    // Parameters:
    //    0) [in] peer: The address of the listening UDT entity.

    Result<> connect(const detail::SocketAddress& remotePeer);

    // Functionality:
    //    Process the response handshake packet.
    // Parameters:
    //    0) [in] pkt: handshake packet.

    Result<ConnectState> connect(const CPacket& pkt);

    // Functionality:
    //    Connect to a UDT entity listening at address "peer", which has sent "hs" request.
    // Parameters:
    //    0) [in] peer: The address of the listening UDT entity.
    //    1) [in/out] hs: The handshake information sent by the peer side (in), negotiated value (out).

    Result<> connect(const detail::SocketAddress& peer, CHandShake* hs);

    Result<> shutdown(int how);

    // Functionality:
    //    Close the opened UDT entity.

    void close();

    // Parameters:
    //    0) [in] data: The address of the application data to be sent.
    //    1) [in] len: The size of the data block.
    // Returned value:
    //    Actual size of data sent.

    Result<int> send(const char* data, int len);

    // Parameters:
    //    0) [out] data: data received.
    //    1) [in] len: The desired size of data to be received.
    // Returned value:
    //    Actual size of data received.

    Result<int> recv(char* data, int len);

    // Functionality:
    //    send a message of a memory block "data" with size of "len".
    // Parameters:
    //    0) [out] data: data received.
    //    1) [in] len: The desired size of data to be received.
    //    2) [in] ttl: the time-to-live of the message.
    //    3) [in] inorder: if the message should be delivered in order.
    // Returned value:
    //    Actual size of data sent.

    Result<int> sendmsg(const char* data, int len, std::chrono::milliseconds ttl, bool inorder);

    // Functionality:
    //    Receive a message to buffer "data".
    // Parameters:
    //    0) [out] data: data received.
    //    1) [in] len: size of the buffer.
    // Returned value:
    //    Actual size of data received.

    Result<int> recvmsg(char* data, int len);

    // Functionality:
    //    Request UDT to send out a file described as "fd", starting from "offset", with size of "size".
    // Parameters:
    //    0) [in] ifs: The input file stream.
    //    1) [in, out] offset: From where to read and send data; output is the new offset when the call returns.
    //    2) [in] size: How many data to be sent.
    //    3) [in] block: size of block per read from disk
    // Returned value:
    //    Actual size of data sent.

    Result<int64_t> sendfile(std::fstream& ifs, int64_t& offset, int64_t size, int block = 366000);

    // Functionality:
    //    Request UDT to receive data into a file described as "fd", starting from "offset", with expected size of "size".
    // Parameters:
    //    0) [out] ofs: The output file stream.
    //    1) [in, out] offset: From where to write data; output is the new offset when the call returns.
    //    2) [in] size: How many data to be received.
    //    3) [in] block: size of block per write to disk
    // Returned value:
    //    Actual size of data received.

    Result<int64_t> recvfile(std::fstream& ofs, int64_t& offset, int64_t size, int block = 7320000);

    // Functionality:
    //    Configure UDT options.
    // Parameters:
    //    0) [in] optName: The enum name of a UDT option.
    //    1) [in] optval: The value to be set.
    //    2) [in] optlen: size of "optval".

    Result<> setOpt(UDTOpt optName, const void* optval, int optlen);

    // Functionality:
    //    Read UDT options.
    // Parameters:
    //    0) [in] optName: The enum name of a UDT option.
    //    1) [in] optval: The value to be returned.
    //    2) [out] optlen: size of "optval".

    Result<> getOpt(UDTOpt optName, void* optval, int& optlen);

    // Functionality:
    //    read the performance data since last sample() call.
    // Parameters:
    //    0) [in, out] perf: pointer to a CPerfMon structure to record the performance data.
    //    1) [in] clear: flag to decide if the local performance trace should be cleared.

    Result<> sample(CPerfMon* perf, bool clear = true);

    // Generation and processing of packets
    void sendCtrl(ControlPacketType pkttype, void* lparam = NULL, void* rparam = NULL, int size = 0);
    void processCtrl(CPacket& ctrlpkt);
    int packData(CPacket& packet, std::chrono::microseconds& ts);
    Result<> processData(Unit unit);

    void checkTimers(bool forceAck);

    // Periodical Rate Control Interval, 10000 microsecond.
    static constexpr auto m_iSYNInterval = std::chrono::microseconds(10000);

    static constexpr int m_iSelfClockInterval = 64;       // ACK interval for self-clocking

    static CUDTUnited* s_UDTUnited;               // UDT global management base

public:
    static constexpr UDTSOCKET INVALID_SOCK = -1;         // invalid socket descriptor
    static constexpr int ERROR = -1;                      // socket api error returned value

private: // Identification
    // Mimimum recv buffer size is 32 packets
    static constexpr int kMinRecvBufferSize = 32;
    //Rcv buffer MUST NOT be bigger than Flight Flag size
    static constexpr int kDefaultRecvBufferSize =
        UDT::kDefaultRecvWindowSize < 8192
        ? UDT::kDefaultRecvWindowSize
        : 8192;

    static constexpr int kDefaultSendBufferSize =
        UDT::kDefaultRecvWindowSize < 8192
        ? UDT::kDefaultRecvWindowSize
        : 8192;

    UDTSOCKET m_SocketId = INVALID_SOCK;                        // UDT socket number
    UDTSockType m_iSockType = UDT_STREAM;                     // Type of the UDT connection (SOCK_STREAM or SOCK_DGRAM)
    UDTSOCKET m_PeerID = INVALID_SOCK;                // peer id, for multiplexer

private: // Packet sizes
    int m_iPktSize = 0;                              // Maximum/regular packet size, in bytes
    int m_iPayloadSize = 0;                          // Maximum/regular payload size, in bytes

private: // Options
    int m_iMSS = UDT::kDefaultMSS;                // Maximum Segment Size, in bytes
    bool m_bSynSending = true;                          // Sending syncronization mode
    bool m_bSynRecving = true;                          // Receiving syncronization mode
    int m_iFlightFlagSize = UDT::kDefaultRecvWindowSize;                       // Maximum number of packets in flight from the peer side
    int m_iSndBufSize = kDefaultSendBufferSize;                           // Maximum UDT sender buffer size
    int m_iRcvBufSize = kDefaultRecvBufferSize;                           // Maximum UDT receiver buffer size
    struct linger m_Linger;                             // Linger information on close
    int m_iUDPSndBufSize = 65536;                        // UDP sending buffer size. TODO #akolesnikov why it is not calculated somehow?
    int m_iUDPRcvBufSize = kDefaultRecvBufferSize * UDT::kDefaultMSS;                        // UDP receiving buffer size
    int m_iIPversion = AF_INET;                            // IP version
    bool m_bRendezvous = false;                          // Rendezvous connection mode
    // sending timeout in milliseconds.
    std::chrono::milliseconds m_sendTimeout = std::chrono::milliseconds(-1);
    // receiving timeout in milliseconds.
    std::chrono::milliseconds m_recvTimeout = std::chrono::milliseconds(-1);
    bool m_bReuseAddr = true;                // reuse an exiting port or not, for UDP multiplexer
    int64_t m_llMaxBW = -1;                // maximum data transfer rate (threshold)

private:
    // congestion control
    std::unique_ptr<CCCVirtualFactory> m_pCCFactory;             // Factory class to create a specific CC instance
    CUDTCC m_congestionControl;                                  // congestion control class
    CCache<CInfoBlock>* m_pCache = nullptr;        // network information cache

    // Status
    volatile bool m_bListening = false;                  // If the UDT entit is listening to connection
    volatile bool m_bConnecting = false;            // The short phase when connect() is called but not yet completed
    volatile bool m_bConnected = false;                  // Whether the connection is on or off
    volatile bool m_bClosing = false;                    // If the UDT entity is closing
    volatile bool m_bShutdown = false;                   // If the peer side has shutdown the connection
    volatile bool m_bBroken = false;                     // If the connection has been broken
    volatile bool m_bPeerHealth = true;                 // If the peer status is normal
    bool m_bOpened = false;                              // If the UDT entity has been opened
    int m_iBrokenCounter = 0;            // a counter (number of GC checks) to let the GC tag this socket as disconnected

    int m_iEXPCount = 1;                // Expiration counter
    int m_iBandwidth = 1;               // Estimated bandwidth, number of packets per second
    std::chrono::microseconds m_iRTT = 10 * m_iSYNInterval;   // RTT, in microseconds
    std::chrono::microseconds m_iRTTVar = (10 * m_iSYNInterval) / 2;    // RTT variance
    int m_iDeliveryRate = 16;                // Packet arrival rate at the receiver side

    // Linger expiration time (for GC to close a socket with data in sending buffer)
    std::chrono::microseconds m_ullLingerExpiration = std::chrono::microseconds::zero();

    CHandShake m_ConnReq;               // connection request
    CHandShake m_ConnRes;               // connection response
    // last time when a connection request is sent.
    std::chrono::microseconds m_llLastReqTime = std::chrono::microseconds::zero();

private: // Sending related data
    std::unique_ptr<SendBuffer> m_pSndBuffer;
    std::unique_ptr<CSndLossList> m_pSndLossList;
    std::unique_ptr<CPktTimeWindow> m_pSndTimeWindow;

    std::chrono::microseconds m_ullInterval = std::chrono::microseconds::zero();             // Inter-packet time.
    std::chrono::microseconds m_ullTimeDiff = std::chrono::microseconds::zero();                      // aggregate difference in inter-packet time

    int m_iFlowWindowSize = 0;              // Flow control window size
    volatile double m_dCongestionWindow = 0.0;         // congestion window size

    volatile int32_t m_iSndLastAck = 0;              // Last ACK received
    volatile int32_t m_iSndLastDataAck = 0;          // The real last ACK that updates the sender buffer and loss list
    volatile int32_t m_iSndCurrSeqNo = 0;            // The largest sequence number that has been sent
    int32_t m_iLastDecSeq = 0;                       // Sequence number sent last decrease occurs
    int32_t m_iSndLastAck2 = 0;                      // Last ACK2 sent back
    // The time when last ACK2 was sent back.
    std::chrono::microseconds m_ullSndLastAck2Time = std::chrono::microseconds::zero();

    int32_t m_iISN = 0;                              // Initial Sequence Number

    void CCUpdate();

private: // Receiving related data
    std::unique_ptr<ReceiveBuffer> m_pRcvBuffer;
    std::unique_ptr<CRcvLossList> m_pRcvLossList;
    std::unique_ptr<CACKWindow> m_pACKWindow;                    // ACK history window
    std::unique_ptr<CPktTimeWindow> m_pRcvTimeWindow;

    int32_t m_iRcvLastAck = 0;                       // Last sent ACK
    std::chrono::microseconds m_ullLastAckTime = std::chrono::microseconds::zero();                   // Timestamp of last ACK
    int32_t m_iRcvLastAckAck = 0;                    // Last sent ACK that has been acknowledged
    int32_t m_iAckSeqNo = 0;                         // Last ACK sequence number
    int32_t m_iRcvCurrSeqNo = 0;                     // Largest received sequence number

    uint64_t m_ullLastWarningTime = 0;               // Last time that a warning message is sent

    int32_t m_iPeerISN = 0;                          // Initial Sequence Number of the peer side

private: // synchronization: mutexes and conditions
    std::mutex m_ConnectionLock;            // used to synchronize connection operation

    std::condition_variable m_SendBlockCond;              // used to block "send" call
    std::mutex m_SendBlockLock;             // lock associated to m_SendBlockCond

    std::mutex m_AckLock;                   // used to protected sender's loss list when processing ACK

    std::condition_variable m_RecvDataCond; // used to block "recv" when there is no data
    std::mutex m_RecvDataLock;              // lock associated to m_RecvDataCond

    std::mutex m_SendLock;                  // used to synchronize "send" call
    std::mutex m_RecvLock;                  // used to synchronize "recv" call

    void releaseSynch();

private: // Trace
    // timestamp when the UDT entity is started.
    std::chrono::microseconds m_StartTime = std::chrono::microseconds::zero();
    int64_t m_llSentTotal = 0;                      // total number of sent data packets, including retransmissions
    int64_t m_llRecvTotal = 0;                      // total number of received packets
    int m_iSndLossTotal = 0;                        // total number of lost packets (sender side)
    int m_iRcvLossTotal = 0;                        // total number of lost packets (receiver side)
    int m_iRetransTotal = 0;                        // total number of retransmitted packets
    int m_iSentACKTotal = 0;                        // total number of sent ACK packets
    int m_iRecvACKTotal = 0;                        // total number of received ACK packets
    int m_iSentNAKTotal = 0;                        // total number of sent NAK packets
    int m_iRecvNAKTotal = 0;                        // total number of received NAK packets
    std::chrono::microseconds m_llSndDurationTotal = std::chrono::microseconds::zero();               // total real time for sending

    // last performance sample time.
    std::chrono::microseconds m_LastSampleTime = std::chrono::microseconds::zero();
    int64_t m_llTraceSent = 0;                      // number of pakctes sent in the last trace interval
    int64_t m_llTraceRecv = 0;                      // number of pakctes received in the last trace interval
    int m_iTraceSndLoss = 0;                        // number of lost packets in the last trace interval (sender side)
    int m_iTraceRcvLoss = 0;                        // number of lost packets in the last trace interval (receiver side)
    int m_iTraceRetrans = 0;                        // number of retransmitted packets in the last trace interval
    int m_iSentACK = 0;                             // number of ACKs sent in the last trace interval
    int m_iRecvACK = 0;                             // number of ACKs received in the last trace interval
    int m_iSentNAK = 0;                             // number of NAKs sent in the last trace interval
    int m_iRecvNAK = 0;                             // number of NAKs received in the last trace interval
    std::chrono::microseconds m_llSndDuration = std::chrono::microseconds::zero();            // real time for sending
    // timers to record the sending duration.
    std::chrono::microseconds m_llSndDurationCounter = std::chrono::microseconds::zero();

private: // Timers
    uint64_t m_ullCPUFrequency = 1;                  // CPU clock frequency, used for Timer, ticks per microsecond

    std::chrono::microseconds m_ullNextACKTime = std::chrono::microseconds::zero();            // Next ACK time, in CPU clock cycles, same below
    std::chrono::microseconds m_ullNextNAKTime = std::chrono::microseconds::zero();            // Next NAK time

    std::chrono::microseconds m_ullSYNInt = std::chrono::microseconds::zero();        // SYN interval
    std::chrono::microseconds m_ullACKInt = std::chrono::microseconds::zero();        // ACK interval
    std::chrono::microseconds m_ullNAKInt = std::chrono::microseconds::zero();        // NAK interval

    // time stamp of last response from the peer.
    std::chrono::microseconds m_ullLastRspTime = std::chrono::microseconds::zero();

    std::chrono::microseconds m_ullMinNakInt = std::chrono::microseconds::zero();            // NAK timeout lower bound; too small value can cause unnecessary retransmission
    std::chrono::microseconds m_ullMinExpInt = std::chrono::microseconds::zero();            // timeout lower bound threshold: too small timeout can cause problem

    int m_iPktCount = 0;                // packet counter for ACK
    int m_iLightACKCount = 0;            // light ACK counter

    std::chrono::microseconds m_ullTargetTime = std::chrono::microseconds::zero();            // scheduled time of next packet sending

private: // for UDP multiplexer
    std::shared_ptr<Multiplexer> m_multiplexer;
    detail::SocketAddress m_pPeerAddr;    // peer address
    uint32_t m_piSelfIP[4];             // local UDP IP address
    std::shared_ptr<ServerSideConnectionAcceptor> m_synPacketHandler;

private: // for epoll
    std::set<int> m_sPollID;            // set of epoll ID to trigger

private:
    void initializeConnectedSocket(const detail::SocketAddress& addr);
};

#endif
