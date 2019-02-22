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

#include <mutex>

#include "udt.h"
#include "common.h"
#include "list.h"
#include "buffer.h"
#include "window.h"
#include "packet.h"
#include "channel.h"
#include "api.h"
#include "ccc.h"
#include "cache.h"
#include "queue.h"

enum UDTSockType { UDT_STREAM = 1, UDT_DGRAM };

class Multiplexer;

/**
 * Processes handshake requests.
 */
class ServerSideConnectionAcceptor
{
public:
    ServerSideConnectionAcceptor(
        uint64_t startTime,
        UDTSockType sockType,
        UDTSOCKET socketId,
        CSndQueue* sndQueue,
        std::set<int> pollIds);

    /**
     * If packet does not contain connection request, non-zero is returned.
     */
    int processConnectionRequest(sockaddr* addr, CPacket& packet);

    void addEPoll(const int eid);
    void removeEPoll(const int eid);

private:
    bool m_closing = false;
    uint64_t m_StartTime = 0;
    UDTSockType m_iSockType;
    UDTSOCKET m_SocketId;
    CSndQueue* m_sendQueue = nullptr;
    std::set<int> m_pollIds;
};

//-------------------------------------------------------------------------------------------------

class CUDT:
    public std::enable_shared_from_this<CUDT>
{
public: //API
    CUDT();
    CUDT(const CUDT& ancestor);
    CUDT& operator=(const CUDT&) = delete;
    ~CUDT();

    static int startup();
    static int cleanup();
    static UDTSOCKET socket(int af, int type = SOCK_STREAM, int protocol = 0);
    static int bind(UDTSOCKET u, const sockaddr* name, int namelen);
    static int bind(UDTSOCKET u, UDPSOCKET udpsock);
    static int listen(UDTSOCKET u, int backlog);
    static UDTSOCKET accept(UDTSOCKET u, sockaddr* addr, int* addrlen);
    static int connect(UDTSOCKET u, const sockaddr* name, int namelen);
    static int shutdown(UDTSOCKET u, int how);
    static int close(UDTSOCKET u);
    static int getpeername(UDTSOCKET u, sockaddr* name, int* namelen);
    static int getsockname(UDTSOCKET u, sockaddr* name, int* namelen);
    static int getsockopt(UDTSOCKET u, int level, UDTOpt optname, void* optval, int* optlen);
    static int setsockopt(UDTSOCKET u, int level, UDTOpt optname, const void* optval, int optlen);
    static int send(UDTSOCKET u, const char* buf, int len, int flags);
    static int recv(UDTSOCKET u, char* buf, int len, int flags);
    static int sendmsg(UDTSOCKET u, const char* buf, int len, int ttl = -1, bool inorder = false);
    static int recvmsg(UDTSOCKET u, char* buf, int len);
    static int64_t sendfile(UDTSOCKET u, std::fstream& ifs, int64_t& offset, int64_t size, int block = 364000);
    static int64_t recvfile(UDTSOCKET u, std::fstream& ofs, int64_t& offset, int64_t size, int block = 7280000);
    static int select(int nfds, ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout);
    static int selectEx(const std::vector<UDTSOCKET>& fds, std::vector<UDTSOCKET>* readfds, std::vector<UDTSOCKET>* writefds, std::vector<UDTSOCKET>* exceptfds, int64_t msTimeOut);
    static int epoll_create();
    static int epoll_add_usock(const int eid, const UDTSOCKET u, const int* events = NULL);
    static int epoll_add_ssock(const int eid, const SYSSOCKET s, const int* events = NULL);
    static int epoll_remove_usock(const int eid, const UDTSOCKET u);
    static int epoll_remove_ssock(const int eid, const SYSSOCKET s);
    static int epoll_wait(
        const int eid,
        std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds, int64_t msTimeOut,
        std::map<SYSSOCKET, int>* lrfds = NULL, std::map<SYSSOCKET, int>* wrfds = NULL);
    static int epoll_interrupt_wait(const int eid);
    static int epoll_release(const int eid);
    static CUDTException& getlasterror();
    static int perfmon(UDTSOCKET u, CPerfMon* perf, bool clear = true);
    static UDTSTATUS getsockstate(UDTSOCKET u);

public: // internal API
    static std::shared_ptr<CUDT> getUDTHandle(UDTSOCKET u);

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

    CSNode* sNode() { return m_pSNode; }

    int payloadSize() const { return m_iPayloadSize; }

    int64_t lastReqTime() const { return m_llLastReqTime; }
    void setLastReqTime(int64_t value) { m_llLastReqTime = value; }

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

    CSndBuffer* sndBuffer() { return m_pSndBuffer; }
    CRcvBuffer* rcvBuffer() { return m_pRcvBuffer; }

    bool synRecving() const { return m_bSynRecving; }
    bool synSending() const { return m_bSynSending; }
    sockaddr* peerAddr() { return m_pPeerAddr; };
    const uint32_t* selfIp() const { return m_piSelfIP; }

    int32_t isn() const { return m_iISN; }
    int mss() const { return m_iMSS; }
    int flightFlagSize() const { return m_iFlightFlagSize; }
    int sndBufSize() const { return m_iSndBufSize; }
    int rcvBufSize() const { return m_iRcvBufSize; }

    struct linger linger() const { return m_Linger; }
    int udpSndBufSize() const { return m_iUDPSndBufSize; }
    int udpRcvBufSize() const { return m_iUDPRcvBufSize; }

    uint64_t lingerExpiration() const { return m_ullLingerExpiration; }
    void setLingerExpiration(uint64_t value) { m_ullLingerExpiration = value; }

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

    void listen();

    // Functionality:
    //    Connect to a UDT entity listening at address "peer".
    // Parameters:
    //    0) [in] peer: The address of the listening UDT entity.
    // Returned value:
    //    None.

    void connect(const sockaddr* peer);

    // Functionality:
    //    Process the response handshake packet.
    // Parameters:
    //    0) [in] pkt: handshake packet.
    // Returned value:
    //    Return 0 if connected, positive value if connection is in progress, otherwise error code.

    int connect(const CPacket& pkt);

    // Functionality:
    //    Connect to a UDT entity listening at address "peer", which has sent "hs" request.
    // Parameters:
    //    0) [in] peer: The address of the listening UDT entity.
    //    1) [in/out] hs: The handshake information sent by the peer side (in), negotiated value (out).
    // Returned value:
    //    None.

    void connect(const sockaddr* peer, CHandShake* hs);

    int shutdown(int how);

    // Functionality:
    //    Close the opened UDT entity.
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    void close();

    // Functionality:
    //    Request UDT to send out a data block "data" with size of "len".
    // Parameters:
    //    0) [in] data: The address of the application data to be sent.
    //    1) [in] len: The size of the data block.
    // Returned value:
    //    Actual size of data sent.

    int send(const char* data, int len);

    // Functionality:
    //    Request UDT to receive data to a memory block "data" with size of "len".
    // Parameters:
    //    0) [out] data: data received.
    //    1) [in] len: The desired size of data to be received.
    // Returned value:
    //    Actual size of data received.

    int recv(char* data, int len);

    // Functionality:
    //    send a message of a memory block "data" with size of "len".
    // Parameters:
    //    0) [out] data: data received.
    //    1) [in] len: The desired size of data to be received.
    //    2) [in] ttl: the time-to-live of the message.
    //    3) [in] inorder: if the message should be delivered in order.
    // Returned value:
    //    Actual size of data sent.

    int sendmsg(const char* data, int len, int ttl, bool inorder);

    // Functionality:
    //    Receive a message to buffer "data".
    // Parameters:
    //    0) [out] data: data received.
    //    1) [in] len: size of the buffer.
    // Returned value:
    //    Actual size of data received.

    int recvmsg(char* data, int len);

    // Functionality:
    //    Request UDT to send out a file described as "fd", starting from "offset", with size of "size".
    // Parameters:
    //    0) [in] ifs: The input file stream.
    //    1) [in, out] offset: From where to read and send data; output is the new offset when the call returns.
    //    2) [in] size: How many data to be sent.
    //    3) [in] block: size of block per read from disk
    // Returned value:
    //    Actual size of data sent.

    int64_t sendfile(std::fstream& ifs, int64_t& offset, int64_t size, int block = 366000);

    // Functionality:
    //    Request UDT to receive data into a file described as "fd", starting from "offset", with expected size of "size".
    // Parameters:
    //    0) [out] ofs: The output file stream.
    //    1) [in, out] offset: From where to write data; output is the new offset when the call returns.
    //    2) [in] size: How many data to be received.
    //    3) [in] block: size of block per write to disk
    // Returned value:
    //    Actual size of data received.

    int64_t recvfile(std::fstream& ofs, int64_t& offset, int64_t size, int block = 7320000);

    // Functionality:
    //    Configure UDT options.
    // Parameters:
    //    0) [in] optName: The enum name of a UDT option.
    //    1) [in] optval: The value to be set.
    //    2) [in] optlen: size of "optval".
    // Returned value:
    //    None.

    void setOpt(UDTOpt optName, const void* optval, int optlen);

    // Functionality:
    //    Read UDT options.
    // Parameters:
    //    0) [in] optName: The enum name of a UDT option.
    //    1) [in] optval: The value to be returned.
    //    2) [out] optlen: size of "optval".
    // Returned value:
    //    None.

    void getOpt(UDTOpt optName, void* optval, int& optlen);

    // Functionality:
    //    read the performance data since last sample() call.
    // Parameters:
    //    0) [in, out] perf: pointer to a CPerfMon structure to record the performance data.
    //    1) [in] clear: flag to decide if the local performance trace should be cleared.
    // Returned value:
    //    None.

    void sample(CPerfMon* perf, bool clear = true);

    // Generation and processing of packets
    void sendCtrl(ControlPacketType pkttype, void* lparam = NULL, void* rparam = NULL, int size = 0);
    void processCtrl(CPacket& ctrlpkt);
    int packData(CPacket& packet, uint64_t& ts);
    int processData(CUnit* unit);

    void checkTimers(bool forceAck);

    static constexpr int m_iSYNInterval = 10000;             // Periodical Rate Control Interval, 10000 microsecond
    static constexpr int m_iSelfClockInterval = 64;       // ACK interval for self-clocking
    static CUDTUnited* s_UDTUnited;               // UDT global management base

public:
    static constexpr UDTSOCKET INVALID_SOCK = -1;         // invalid socket descriptor
    static constexpr int ERROR = -1;                      // socket api error returned value
    static constexpr int m_iVersion = 4;                 // UDT version, for compatibility use

private: // Identification
    static constexpr int kDefaultRecvWindowSize = 25600;

    // Mimimum recv buffer size is 32 packets
    static constexpr int kMinRecvBufferSize = 32;
    //Rcv buffer MUST NOT be bigger than Flight Flag size
    static constexpr int kDefaultRecvBufferSize =
        kDefaultRecvWindowSize < 8192
        ? kDefaultRecvWindowSize
        : 8192;

    static constexpr int kDefaultSendBufferSize =
        kDefaultRecvWindowSize < 8192
        ? kDefaultRecvWindowSize
        : 8192;

    UDTSOCKET m_SocketId = INVALID_SOCK;                        // UDT socket number
    UDTSockType m_iSockType = UDT_STREAM;                     // Type of the UDT connection (SOCK_STREAM or SOCK_DGRAM)
    UDTSOCKET m_PeerID = INVALID_SOCK;                // peer id, for multiplexer

private: // Packet sizes
    int m_iPktSize = 0;                              // Maximum/regular packet size, in bytes
    int m_iPayloadSize = 0;                          // Maximum/regular payload size, in bytes

private: // Options
    int m_iMSS = kDefaultMtuSize;                // Maximum Segment Size, in bytes
    bool m_bSynSending = true;                          // Sending syncronization mode
    bool m_bSynRecving = true;                          // Receiving syncronization mode
    int m_iFlightFlagSize = kDefaultRecvWindowSize;                       // Maximum number of packets in flight from the peer side
    int m_iSndBufSize = kDefaultSendBufferSize;                           // Maximum UDT sender buffer size
    int m_iRcvBufSize = kDefaultRecvBufferSize;                           // Maximum UDT receiver buffer size
    struct linger m_Linger;                             // Linger information on close
    int m_iUDPSndBufSize = 65536;                        // UDP sending buffer size. TODO #ak why it is not calculated somehow?
    int m_iUDPRcvBufSize = kDefaultRecvBufferSize * kDefaultMtuSize;                        // UDP receiving buffer size
    int m_iIPversion = AF_INET;                            // IP version
    bool m_bRendezvous = false;                          // Rendezvous connection mode
    int m_iSndTimeOut = -1;                           // sending timeout in milliseconds
    int m_iRcvTimeOut = -1;                           // receiving timeout in milliseconds
    bool m_bReuseAddr = true;                // reuse an exiting port or not, for UDP multiplexer
    int64_t m_llMaxBW = -1;                // maximum data transfer rate (threshold)

private: // congestion control
    CCCVirtualFactory* m_pCCFactory = nullptr;             // Factory class to create a specific CC instance
    CCC* m_pCC = nullptr;                                  // congestion control class
    CCache<CInfoBlock>* m_pCache = nullptr;        // network information cache

private: // Status
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
    int m_iRTT = 10 * m_iSYNInterval;   // RTT, in microseconds
    int m_iRTTVar = (10 * m_iSYNInterval) >> 1;                      // RTT variance
    int m_iDeliveryRate = 16;                // Packet arrival rate at the receiver side

    uint64_t m_ullLingerExpiration = 0;     // Linger expiration time (for GC to close a socket with data in sending buffer)

    CHandShake m_ConnReq;               // connection request
    CHandShake m_ConnRes;               // connection response
    int64_t m_llLastReqTime = 0;            // last time when a connection request is sent

private: // Sending related data
    CSndBuffer* m_pSndBuffer = nullptr;                    // Sender buffer
    CSndLossList* m_pSndLossList = nullptr;                // Sender loss list
    CPktTimeWindow* m_pSndTimeWindow = nullptr;            // Packet sending time window

    volatile uint64_t m_ullInterval = 0;             // Inter-packet time, in CPU clock cycles
    uint64_t m_ullTimeDiff = 0;                      // aggregate difference in inter-packet time

    volatile int m_iFlowWindowSize = 0;              // Flow control window size
    volatile double m_dCongestionWindow = 0.0;         // congestion window size

    volatile int32_t m_iSndLastAck = 0;              // Last ACK received
    volatile int32_t m_iSndLastDataAck = 0;          // The real last ACK that updates the sender buffer and loss list
    volatile int32_t m_iSndCurrSeqNo = 0;            // The largest sequence number that has been sent
    int32_t m_iLastDecSeq = 0;                       // Sequence number sent last decrease occurs
    int32_t m_iSndLastAck2 = 0;                      // Last ACK2 sent back
    uint64_t m_ullSndLastAck2Time = 0;               // The time when last ACK2 was sent back

    int32_t m_iISN = 0;                              // Initial Sequence Number

    void CCUpdate();

private: // Receiving related data
    CRcvBuffer* m_pRcvBuffer = nullptr;                    // Receiver buffer
    CRcvLossList* m_pRcvLossList = nullptr;                // Receiver loss list
    CACKWindow* m_pACKWindow = nullptr;                    // ACK history window
    CPktTimeWindow* m_pRcvTimeWindow = nullptr;            // Packet arrival time window

    int32_t m_iRcvLastAck = 0;                       // Last sent ACK
    uint64_t m_ullLastAckTime = 0;                   // Timestamp of last ACK
    int32_t m_iRcvLastAckAck = 0;                    // Last sent ACK that has been acknowledged
    int32_t m_iAckSeqNo = 0;                         // Last ACK sequence number
    int32_t m_iRcvCurrSeqNo = 0;                     // Largest received sequence number

    uint64_t m_ullLastWarningTime = 0;               // Last time that a warning message is sent

    int32_t m_iPeerISN = 0;                          // Initial Sequence Number of the peer side

private: // synchronization: mutexes and conditions
    std::mutex m_ConnectionLock;            // used to synchronize connection operation

    pthread_cond_t m_SendBlockCond;              // used to block "send" call
    pthread_mutex_t m_SendBlockLock;             // lock associated to m_SendBlockCond

    pthread_mutex_t m_AckLock;                   // used to protected sender's loss list when processing ACK

    pthread_cond_t m_RecvDataCond;               // used to block "recv" when there is no data
    pthread_mutex_t m_RecvDataLock;              // lock associated to m_RecvDataCond

    pthread_mutex_t m_SendLock;                  // used to synchronize "send" call
    std::mutex m_RecvLock;                  // used to synchronize "recv" call

    void initSynch();
    void destroySynch();
    void releaseSynch();

private: // Trace
    uint64_t m_StartTime = 0;                       // timestamp when the UDT entity is started
    int64_t m_llSentTotal = 0;                      // total number of sent data packets, including retransmissions
    int64_t m_llRecvTotal = 0;                      // total number of received packets
    int m_iSndLossTotal = 0;                        // total number of lost packets (sender side)
    int m_iRcvLossTotal = 0;                        // total number of lost packets (receiver side)
    int m_iRetransTotal = 0;                        // total number of retransmitted packets
    int m_iSentACKTotal = 0;                        // total number of sent ACK packets
    int m_iRecvACKTotal = 0;                        // total number of received ACK packets
    int m_iSentNAKTotal = 0;                        // total number of sent NAK packets
    int m_iRecvNAKTotal = 0;                        // total number of received NAK packets
    int64_t m_llSndDurationTotal = 0;               // total real time for sending

    uint64_t m_LastSampleTime = 0;                  // last performance sample time
    int64_t m_llTraceSent = 0;                      // number of pakctes sent in the last trace interval
    int64_t m_llTraceRecv = 0;                      // number of pakctes received in the last trace interval
    int m_iTraceSndLoss = 0;                        // number of lost packets in the last trace interval (sender side)
    int m_iTraceRcvLoss = 0;                        // number of lost packets in the last trace interval (receiver side)
    int m_iTraceRetrans = 0;                        // number of retransmitted packets in the last trace interval
    int m_iSentACK = 0;                             // number of ACKs sent in the last trace interval
    int m_iRecvACK = 0;                             // number of ACKs received in the last trace interval
    int m_iSentNAK = 0;                             // number of NAKs sent in the last trace interval
    int m_iRecvNAK = 0;                             // number of NAKs received in the last trace interval
    int64_t m_llSndDuration = 0;            // real time for sending
    int64_t m_llSndDurationCounter = 0;        // timers to record the sending duration

private: // Timers
    uint64_t m_ullCPUFrequency = 1;                  // CPU clock frequency, used for Timer, ticks per microsecond

    uint64_t m_ullNextACKTime = 0;            // Next ACK time, in CPU clock cycles, same below
    uint64_t m_ullNextNAKTime = 0;            // Next NAK time

    volatile uint64_t m_ullSYNInt = 0;        // SYN interval
    volatile uint64_t m_ullACKInt = 0;        // ACK interval
    volatile uint64_t m_ullNAKInt = 0;        // NAK interval
    volatile uint64_t m_ullLastRspTime = 0;        // time stamp of last response from the peer

    uint64_t m_ullMinNakInt = 0;            // NAK timeout lower bound; too small value can cause unnecessary retransmission
    uint64_t m_ullMinExpInt = 0;            // timeout lower bound threshold: too small timeout can cause problem

    int m_iPktCount = 0;                // packet counter for ACK
    int m_iLightACKCount = 0;            // light ACK counter

    uint64_t m_ullTargetTime = 0;            // scheduled time of next packet sending

private: // for UDP multiplexer
    std::shared_ptr<Multiplexer> m_multiplexer;
    sockaddr* m_pPeerAddr = nullptr;    // peer address
    uint32_t m_piSelfIP[4];             // local UDP IP address
    CSNode* m_pSNode = nullptr;         // node information for UDT list used in snd queue
    std::shared_ptr<ServerSideConnectionAcceptor> m_synPacketHandler;

private: // for epoll
    std::set<int> m_sPollID;            // set of epoll ID to trigger
};

#endif
