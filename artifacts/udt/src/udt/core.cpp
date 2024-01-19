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

#ifndef _WIN32
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef LEGACY_WIN32
#include <wspiapi.h>
#endif
#endif
#include <atomic>
#include <cmath>
#include <sstream>
#include "queue.h"
#include "core.h"
#include "multiplexer.h"

using namespace std;


CUDTUnited* CUDT::s_UDTUnited = nullptr;

const UDTSOCKET UDT::INVALID_SOCK = CUDT::INVALID_SOCK;
const int UDT::ERROR = CUDT::ERROR;

const int32_t CSeqNo::m_iSeqNoTH = 0x3FFFFFFF;
const int32_t CSeqNo::m_iMaxSeqNo = 0x7FFFFFFF;
const int32_t CAckNo::m_iMaxAckSeqNo = 0x7FFFFFFF;
const int32_t CMsgNo::m_iMsgNoTH = 0xFFFFFFF;
const int32_t CMsgNo::m_iMaxMsgNo = 0x1FFFFFFF;

static constexpr auto kConnectionBrokenTimeout = std::chrono::seconds(5);


/*
static const int CTRL_HANDSHAKE = 0;            //000 - Handshake
static const int CTRL_KEEP_ALIVE = 1;           //001 - Keep-alive
static const int CTRL_ACK = 2;                  //010 - Acknowledgement
static const int CTRL_LOSS_REPORT = 3;          //011 - Loss Report
static const int CTRL_CONGESTION_WARNING = 4;   //100 - Congestion Warning
static const int CTRL_SHUTDOWN = 5;             //101 - Shutdown
static const int CTRL_ACK2 = 6;                 //110 - Acknowledgement of Acknowledgement
static const int CTRL_MSG_DROP = 7;             //111 - Msg drop request
static const int CTRL_ACK_SPECIAL_ERROR = 8;    //1000 - acknowledge the peer side a special error
static const int CTRL_RESERVED = 32767;         //0x7FFF - Resevered for future use

// Mimimum recv flight flag size is 32 packets
constexpr const int kMinRecvWindowSize = 32;
*/

CUDT::CUDT()
{
    memset(m_piSelfIP, 0, sizeof(m_piSelfIP));

    m_Linger.l_onoff = 1;
    m_Linger.l_linger = 180;

    m_pCCFactory = std::make_unique<CCCFactory<CUDTCC>>();
}

CUDT::CUDT(const CUDT& ancestor):
    std::enable_shared_from_this<CUDT>()
{
    // Default UDT configurations
    m_iMSS = ancestor.m_iMSS;
    m_bSynSending = ancestor.m_bSynSending;
    m_bSynRecving = ancestor.m_bSynRecving;
    m_iFlightFlagSize = ancestor.m_iFlightFlagSize;
    m_iSndBufSize = ancestor.m_iSndBufSize;
    m_iRcvBufSize = ancestor.m_iRcvBufSize;
    m_Linger = ancestor.m_Linger;
    m_iUDPSndBufSize = ancestor.m_iUDPSndBufSize;
    m_iUDPRcvBufSize = ancestor.m_iUDPRcvBufSize;
    m_iSockType = ancestor.m_iSockType;
    m_iIPversion = ancestor.m_iIPversion;
    m_bRendezvous = ancestor.m_bRendezvous;
    m_sendTimeout = ancestor.m_sendTimeout;
    m_recvTimeout = ancestor.m_recvTimeout;
    m_bReuseAddr = true;    // this must be true, because all accepted sockets shared the same port with the listener
    m_llMaxBW = ancestor.m_llMaxBW;

    m_pCCFactory = ancestor.m_pCCFactory->clone();
    m_pCache = ancestor.m_pCache;
}

CUDT::~CUDT()
{
    // NOTE: #muskov Not sure how it is supposed to work but close() does not remove this pointer
    //   from sending queue in case of async IO and enabled linger...
    //
    // This workaround is here to prevent a segfault:
    if (m_multiplexer)
        m_multiplexer->sendQueue().sndUList().remove(this);

    // destroy the data structures
    m_pSndBuffer.reset();
    m_pRcvBuffer.reset();
    m_pSndLossList.reset();
    m_pRcvLossList.reset();
    m_pACKWindow.reset();
    m_pSndTimeWindow.reset();
    m_pRcvTimeWindow.reset();
}

CSndQueue& CUDT::sndQueue()
{
    return m_multiplexer->sendQueue();
}

CRcvQueue& CUDT::rcvQueue()
{
    return m_multiplexer->recvQueue();
}

void CUDT::setMultiplexer(const std::shared_ptr<Multiplexer>& multiplexer)
{
    m_multiplexer = multiplexer;
}

Result<> CUDT::setOpt(UDTOpt optName, const void* optval, int)
{
    // NOTE: Socket options can be set even if remote side has closed connection.

    //applying options that do not require synchronization
    switch (optName)
    {
        case UDT_LINGER:
            m_Linger = *(struct linger*)optval;
            return success();

        default:
            break;
    }

    std::scoped_lock lock(m_ConnectionLock, m_SendLock, m_RecvLock);

    switch (optName)
    {
        case UDT_MSS:
            if (m_bOpened)
                return Error(OsErrorCode::invalidData);

            if (*(int*)optval < int(28 + CHandShake::m_iContentSize))
                return Error(OsErrorCode::invalidData);

            m_iMSS = *(int*)optval;

            // Packet size cannot be greater than UDP buffer size
            if (m_iMSS > m_iUDPSndBufSize)
                m_iMSS = m_iUDPSndBufSize;
            if (m_iMSS > m_iUDPRcvBufSize)
                m_iMSS = m_iUDPRcvBufSize;

            break;

        case UDT_SNDSYN:
            m_bSynSending = *(bool *)optval;
            break;

        case UDT_RCVSYN:
            m_bSynRecving = *(bool *)optval;
            break;

        case UDT_FC:
            if (isConnecting() || m_bConnected)
                return Error(OsErrorCode::isConnected);

            if (*(int*)optval < 1)
                return Error(OsErrorCode::invalidData);

            if (*(int*)optval > kDefaultRecvWindowSize)
                m_iFlightFlagSize = *(int*)optval;
            else
                m_iFlightFlagSize = kDefaultRecvWindowSize;

            break;

        case UDT_SNDBUF:
            if (m_bOpened)
                return Error(OsErrorCode::isConnected);

            if (*(int*)optval <= 0)
                return Error(OsErrorCode::invalidData);

            m_iSndBufSize = *(int*)optval / (m_iMSS - 28);
            break;

        case UDT_RCVBUF:
            if (m_bOpened)
                return Error(OsErrorCode::isConnected);

            if (*(int*)optval <= 0)
                return Error(OsErrorCode::invalidData);

            if (*(int*)optval > (m_iMSS - 28) * kMinRecvBufferSize)
                m_iRcvBufSize = *(int*)optval / (m_iMSS - 28);
            else
                m_iRcvBufSize = kMinRecvBufferSize;

            // recv buffer MUST not be greater than FC size
            if (m_iRcvBufSize > m_iFlightFlagSize)
                m_iRcvBufSize = m_iFlightFlagSize;

            break;

        case UDP_SNDBUF:
            if (m_bOpened)
                return Error(OsErrorCode::isConnected);

            m_iUDPSndBufSize = *(int*)optval;

            if (m_iUDPSndBufSize < m_iMSS)
                m_iUDPSndBufSize = m_iMSS;

            break;

        case UDP_RCVBUF:
            if (m_bOpened)
                return Error(OsErrorCode::isConnected);

            m_iUDPRcvBufSize = *(int*)optval;

            if (m_iUDPRcvBufSize < m_iMSS)
                m_iUDPRcvBufSize = m_iMSS;

            break;

        case UDT_RENDEZVOUS:
            if (isConnecting() || m_bConnected)
                return Error(OsErrorCode::isConnected);
            m_bRendezvous = *(bool *)optval;
            break;

        case UDT_SNDTIMEO:
            m_sendTimeout = std::chrono::milliseconds(*(int*)optval);
            break;

        case UDT_RCVTIMEO:
            m_recvTimeout = std::chrono::milliseconds(*(int*)optval);
            break;

        case UDT_REUSEADDR:
            if (m_bOpened)
                return Error(OsErrorCode::isConnected);
            m_bReuseAddr = *(bool*)optval;
            break;

        case UDT_MAXBW:
            m_llMaxBW = *(int64_t*)optval;
            break;

        default:
            return Error(OsErrorCode::noProtocolOption);
    }

    return success();
}

Result<> CUDT::getOpt(UDTOpt optName, void* optval, int& optlen)
{
    std::lock_guard<std::mutex> cg(m_ConnectionLock);

    switch (optName)
    {
        case UDT_MSS:
            *(int*)optval = m_iMSS;
            optlen = sizeof(int);
            break;

        case UDT_SNDSYN:
            *(bool*)optval = m_bSynSending;
            optlen = sizeof(bool);
            break;

        case UDT_RCVSYN:
            *(bool*)optval = m_bSynRecving;
            optlen = sizeof(bool);
            break;

        case UDT_FC:
            *(int*)optval = m_iFlightFlagSize;
            optlen = sizeof(int);
            break;

        case UDT_SNDBUF:
            *(int*)optval = m_iSndBufSize * (m_iMSS - 28);
            optlen = sizeof(int);
            break;

        case UDT_RCVBUF:
            *(int*)optval = m_iRcvBufSize * (m_iMSS - 28);
            optlen = sizeof(int);
            break;

        case UDT_LINGER:
            if (optlen < (int)(sizeof(m_Linger)))
                return Error(OsErrorCode::invalidData);

            *(struct linger*)optval = m_Linger;
            optlen = sizeof(m_Linger);
            break;

        case UDP_SNDBUF:
            *(int*)optval = m_iUDPSndBufSize;
            optlen = sizeof(int);
            break;

        case UDP_RCVBUF:
            *(int*)optval = m_iUDPRcvBufSize;
            optlen = sizeof(int);
            break;

        case UDT_RENDEZVOUS:
            *(bool *)optval = m_bRendezvous;
            optlen = sizeof(bool);
            break;

        case UDT_SNDTIMEO:
            *(int*)optval = m_sendTimeout.count();
            optlen = sizeof(int);
            break;

        case UDT_RCVTIMEO:
            *(int*)optval = m_recvTimeout.count();
            optlen = sizeof(int);
            break;

        case UDT_REUSEADDR:
            *(bool *)optval = m_bReuseAddr;
            optlen = sizeof(bool);
            break;

        case UDT_MAXBW:
            *(int64_t*)optval = m_llMaxBW;
            optlen = sizeof(int64_t);
            break;

        case UDT_STATE:
            *(int32_t*)optval = s_UDTUnited->getStatus(m_SocketId);
            optlen = sizeof(int32_t);
            break;

        case UDT_EVENT:
        {
            int32_t event = 0;
            if (m_bBroken)
                event |= UDT_EPOLL_ERR;
            else
            {
                if (m_pRcvBuffer && (m_pRcvBuffer->getRcvDataSize() > 0))
                    event |= UDT_EPOLL_IN;
                if (m_pSndBuffer && (m_iSndBufSize > m_pSndBuffer->getCurrBufSize()))
                    event |= UDT_EPOLL_OUT;
            }
            *(int32_t*)optval = event;
            optlen = sizeof(int32_t);
            break;
        }

        case UDT_SNDDATA:
            if (m_pSndBuffer)
                *(int32_t*)optval = m_pSndBuffer->getCurrBufSize();
            else
                *(int32_t*)optval = 0;
            optlen = sizeof(int32_t);
            break;

        case UDT_RCVDATA:
            if (m_pRcvBuffer)
                *(int32_t*)optval = m_pRcvBuffer->getRcvDataSize();
            else
                *(int32_t*)optval = 0;
            optlen = sizeof(int32_t);
            break;

        default:
            return Error(OsErrorCode::noProtocolOption);
    }

    return success();
}

void CUDT::setConnecting(bool val)
{
    m_bConnecting = val;
}

bool CUDT::isConnecting() const
{
    return m_bConnecting;
}

void CUDT::setIsClosing(bool val)
{
    m_bClosing = val;
}

bool CUDT::isClosing() const
{
    return m_bClosing;
}

void CUDT::setBroken(bool val)
{
    m_bBroken = val;
}

bool CUDT::broken() const
{
    return m_bBroken;
}

void CUDT::open()
{
    std::lock_guard<std::mutex> cg(m_ConnectionLock);

    // Initial sequence number, loss, acknowledgement, etc.
    m_iPktSize = m_iMSS - 28;
    m_iPayloadSize = m_iPktSize - kPacketHeaderSize;

    m_iEXPCount = 1;
    m_iBandwidth = 1;
    m_iAckSeqNo = 0;
    m_ullLastAckTime = std::chrono::microseconds::zero();

    // trace information
    m_StartTime = CTimer::getTime();
    m_llSentTotal = m_llRecvTotal = m_iSndLossTotal = m_iRcvLossTotal = m_iRetransTotal = m_iSentACKTotal = m_iRecvACKTotal = m_iSentNAKTotal = m_iRecvNAKTotal = 0;
    m_LastSampleTime = CTimer::getTime();
    m_llTraceSent = m_llTraceRecv = m_iTraceSndLoss = m_iTraceRcvLoss = m_iTraceRetrans = m_iSentACK = m_iRecvACK = m_iSentNAK = m_iRecvNAK = 0;
    m_llSndDuration = m_llSndDurationTotal = std::chrono::microseconds::zero();

    m_iRTT = 10 * m_iSYNInterval;
    m_iRTTVar = m_iRTT / 2;
    m_ullCPUFrequency = 1; // CTimer::getCPUFrequency();

    // set up the timers
    m_ullSYNInt = m_iSYNInterval;

    m_ullMinNakInt = std::chrono::milliseconds(300);
    m_ullMinExpInt = std::chrono::milliseconds(300);

    m_ullACKInt = m_ullSYNInt;
    m_ullNAKInt = m_ullMinNakInt;

    const auto currtime = CTimer::getTime();
    m_ullLastRspTime = currtime;
    m_ullNextACKTime = currtime + m_ullSYNInt;
    m_ullNextNAKTime = currtime + m_ullNAKInt;

    m_iPktCount = 0;
    m_iLightACKCount = 1;

    m_ullTargetTime = std::chrono::microseconds::zero();
    m_ullTimeDiff = std::chrono::microseconds::zero();

    // Now UDT is opened.
    m_bOpened = true;
}

Result<> CUDT::listen()
{
    std::lock_guard<std::mutex> cg(m_ConnectionLock);

    if (!m_bOpened)
        return Error(OsErrorCode::badDescriptor);

    if (isConnecting() || m_bConnected)
        return Error(OsErrorCode::isConnected);

    // listen can be called more than once
    if (m_bListening)
        return success();

    m_synPacketHandler = std::make_shared<ServerSideConnectionAcceptor>(
        m_StartTime,
        m_iSockType,
        m_SocketId,
        &m_multiplexer->sendQueue(),
        m_sPollID);
    // if there is already another socket listening on the same port
    if (!rcvQueue().setListener(m_synPacketHandler))
        return Error(OsErrorCode::addressInUse);

    m_bListening = true;

    return success();
}

Result<> CUDT::connect(const detail::SocketAddress& serv_addr)
{
    std::lock_guard<std::mutex> cg(m_ConnectionLock);

    if (!m_bOpened)
        return Error(OsErrorCode::badDescriptor);

    if (m_bListening)
        return Error(OsErrorCode::notSupported);

    if (isConnecting() || m_bConnected)
        return Error(OsErrorCode::isConnected);

    if (serv_addr.family() != m_iIPversion)
        return Error(OsErrorCode::invalidData);

    // record peer/server address
    m_pPeerAddr = serv_addr;

    // register this socket in the rendezvous queue
    // RendezevousQueue is used to temporarily store incoming handshake, non-rendezvous connections also require this function
    std::optional<std::chrono::microseconds> deadline;
    if (m_sendTimeout > std::chrono::milliseconds::zero())
        deadline = CTimer::getTime() + m_sendTimeout;
    // else no timeout.

    rcvQueue().registerConnector(
        m_SocketId, shared_from_this(),
        serv_addr,
        deadline ? *deadline : std::chrono::microseconds::max());

    // This is my current configurations
    m_ConnReq.m_iVersion = m_iVersion;
    m_ConnReq.m_iType = m_iSockType;
    m_ConnReq.m_iMSS = m_iMSS;
    m_ConnReq.m_iFlightFlagSize = (m_iRcvBufSize < m_iFlightFlagSize) ? m_iRcvBufSize : m_iFlightFlagSize;
    m_ConnReq.m_iReqType = (!m_bRendezvous) ? 1 : 0;
    m_ConnReq.m_iID = m_SocketId;

    serv_addr.copy(m_ConnReq.m_piPeerIP);

    // Random Initial Sequence Number
    srand(CTimer::getTime().count());
    m_iISN = m_ConnReq.m_iISN = (int32_t)(CSeqNo::m_iMaxSeqNo * (double(rand()) / RAND_MAX));

    m_iLastDecSeq = m_iISN - 1;
    m_iSndLastAck = m_iISN;
    m_iSndLastDataAck = m_iISN;
    m_iSndCurrSeqNo = m_iISN - 1;
    m_iSndLastAck2 = m_iISN;
    m_ullSndLastAck2Time = CTimer::getTime();

    // Inform the server my configurations.
    {
        CPacket request;
        request.pack(ControlPacketType::Handshake, NULL, m_iPayloadSize);
        // ID = 0, connection request
        request.m_iID = 0;

        int hs_size = m_iPayloadSize;
        m_ConnReq.serialize(request.payload().data(), hs_size);
        request.setLength(hs_size);
        sndQueue().sendto(serv_addr, std::move(request));
    }

    m_llLastReqTime = CTimer::getTime();

    // ATTENTION: In the original UDT library the line corresponding to this action has been moved
    // to the beginning of this function right after producing various errors. This fix was done in
    // the commit 6b4216d8a4c9 with the following commit message:
    // ```
    // fix setting of CONNECTING status inside connect().
    // ````
    setConnecting(true);

    // asynchronous connect, return immediately
    if (!m_bSynRecving)
        return success();

    // Wait for the negotiated configurations from the peer side.
    CPacket response;
    response.pack(ControlPacketType::Handshake, NULL, m_iPayloadSize);

    Error error(OsErrorCode::ok);
    Result<> internalConnectResult;

    while (!isClosing() && !broken())
    {
        // avoid sending too many requests, at most 1 request per 250ms
        if (CTimer::getTime() - m_llLastReqTime > kSyncRepeatMinPeriod)
        {
            CPacket request;
            request.pack(ControlPacketType::Handshake, NULL, m_iPayloadSize);
            // ID = 0, connection request
            request.m_iID = 0;

            int hs_size = m_iPayloadSize;
            m_ConnReq.serialize(request.payload().data(), hs_size);
            request.setLength(hs_size);
            if (m_bRendezvous)
                request.m_iID = m_ConnRes.m_iID;
            sndQueue().sendto(serv_addr, std::move(request));
            m_llLastReqTime = CTimer::getTime();
        }

        std::chrono::microseconds recvTimeout = CRcvQueue::kDefaultRecvTimeout;
        if (deadline)
        {
            recvTimeout = std::min(
                std::max(*deadline - CTimer::getTime(), std::chrono::microseconds::zero()),
                recvTimeout);
        }

        response.setLength(m_iPayloadSize);
        if (rcvQueue().recvfrom(m_SocketId, response, recvTimeout) > 0)
        {
            auto result = connect(response);
            if (!result.ok())
            {
                internalConnectResult = Result<>(result.error());
                break;
            }

            if (result.get() == ConnectState::done)
                break;

            // new request/response should be sent out immediately on receving a response
            m_llLastReqTime = std::chrono::microseconds::zero();
        }

        if (deadline && CTimer::getTime() > *deadline)
        {
            // timeout
            error = Error(OsErrorCode::timedOut);
            break;
        }
    }

    if (error.osError() == OsErrorCode::ok)
    {
        if (isClosing())                                                 // if the socket is closed before connection...
            error = Error(OsErrorCode::connectionRefused);
        else if (1002 == m_ConnRes.m_iReqType)                          // connection request rejected
            error = Error(OsErrorCode::connectionRefused);
        else if ((!m_bRendezvous) && (m_iISN != m_ConnRes.m_iISN))      // secuity check
            error = Error(OsErrorCode::connectionRefused, UDT::ProtocolError::handshakeFailure);
        else if (!internalConnectResult.ok()) // Otherwise, success will be reported to the caller
            error = internalConnectResult.error();
    }

    if (error.osError() != OsErrorCode::ok)
        return error;

    return success();
}

Result<ConnectState> CUDT::connect(const CPacket& response)
{
    // this is the 2nd half of a connection request. If the connection is setup successfully this returns 0.
    // returning -1 means there is an error.
    // returning 1 or 2 means the connection is in process and needs more handshake

    if (!isConnecting())
        return Error(OsErrorCode::connectionRefused);

    //a data packet or a keep-alive packet comes, which means the peer side is already connected
    // in this situation, the previously recorded response will be used
    const auto isRemotePeerAlreadyConnected =
        /*m_bRendezvous &&*/
        ((PacketFlag::Data == response.getFlag()) || (ControlPacketType::KeepAlive == response.getType())) &&
        (0 != m_ConnRes.m_iType);

    if (!isRemotePeerAlreadyConnected)
    {
        if ((PacketFlag::Control != response.getFlag()) || (ControlPacketType::Handshake != response.getType()))
            return Error(OsErrorCode::connectionRefused, UDT::ProtocolError::handshakeFailure);

        m_ConnRes.deserialize(response.payload().data(), response.getLength());

        if (m_bRendezvous)
        {
            // regular connect should NOT communicate with rendezvous connect
            // rendezvous connect require 3-way handshake
            if (1 == m_ConnRes.m_iReqType)
            {
                return Error(
                    OsErrorCode::connectionRefused,
                    UDT::ProtocolError::remotePeerInRendezvousMode);
            }

            if ((0 == m_ConnReq.m_iReqType) || (0 == m_ConnRes.m_iReqType))
            {
                m_ConnReq.m_iReqType = -1;
                // the request time must be updated so that the next handshake can be sent out immediately.
                m_llLastReqTime = std::chrono::microseconds::zero();
                return Result<ConnectState>(ConnectState::inProgress);
            }
        }
        else
        {
            // set cookie
            if (1 == m_ConnRes.m_iReqType)
            {
                m_ConnReq.m_iReqType = -1;
                m_ConnReq.m_iCookie = m_ConnRes.m_iCookie;
                m_llLastReqTime = std::chrono::microseconds::zero();
                return Result<ConnectState>(ConnectState::inProgress);
            }
        }
    }

    // Remove from rendezvous queue
    rcvQueue().removeConnector(m_SocketId);

    // Re-configure according to the negotiated values.
    m_iMSS = m_ConnRes.m_iMSS;
    m_iFlowWindowSize = m_ConnRes.m_iFlightFlagSize;
    m_iPeerISN = m_ConnRes.m_iISN;
    m_iRcvLastAck = m_ConnRes.m_iISN;
    m_iRcvLastAckAck = m_ConnRes.m_iISN;
    m_iRcvCurrSeqNo = m_ConnRes.m_iISN - 1;
    m_PeerID = m_ConnRes.m_iID;
    memcpy(m_piSelfIP, m_ConnRes.m_piPeerIP, 16);

    setConnecting(false);

    initializeConnectedSocket(m_pPeerAddr);

    // acknowledge the management module.
    if (auto result = s_UDTUnited->connect_complete(m_SocketId); !result.ok())
        return result.error();

    // acknowledde any waiting epolls to write
    s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_OUT, true);

    return Result<ConnectState>(ConnectState::done);
}

Result<> CUDT::connect(const detail::SocketAddress& peer, CHandShake* hs)
{
    std::lock_guard<std::mutex> cg(m_ConnectionLock);

    // Uses the smaller MSS between the peers
    if (hs->m_iMSS > m_iMSS)
        hs->m_iMSS = m_iMSS;
    else
        m_iMSS = hs->m_iMSS;

    // exchange info for maximum flow window size
    m_iFlowWindowSize = hs->m_iFlightFlagSize;
    hs->m_iFlightFlagSize = (m_iRcvBufSize < m_iFlightFlagSize) ? m_iRcvBufSize : m_iFlightFlagSize;

    m_iPeerISN = hs->m_iISN;

    m_iRcvLastAck = hs->m_iISN;
    m_iRcvLastAckAck = hs->m_iISN;
    m_iRcvCurrSeqNo = hs->m_iISN - 1;

    m_PeerID = hs->m_iID;
    hs->m_iID = m_SocketId;

    // use peer's ISN and send it back for security check
    m_iISN = hs->m_iISN;

    m_iLastDecSeq = m_iISN - 1;
    m_iSndLastAck = m_iISN;
    m_iSndLastDataAck = m_iISN;
    m_iSndCurrSeqNo = m_iISN - 1;
    m_iSndLastAck2 = m_iISN;
    m_ullSndLastAck2Time = CTimer::getTime();

    // this is a reponse handshake
    hs->m_iReqType = -1;

    // get local IP address and send the peer its IP address (because UDP cannot get local IP address)
    memcpy(m_piSelfIP, hs->m_piPeerIP, 16);
    peer.copy(hs->m_piPeerIP);

    m_iIPversion = peer.family();
    m_pPeerAddr = peer;

    initializeConnectedSocket(peer);

    //send the response to the peer, see listen() for more discussions about this
    CPacket response;
    int size = CHandShake::m_iContentSize;
    response.pack(ControlPacketType::Handshake, NULL, size);
    hs->serialize(response.payload().data(), size);
    response.m_iID = m_PeerID;
    sndQueue().sendto(peer, std::move(response));

    return success();
}

Result<> CUDT::shutdown(int /*how*/)
{
    m_bBroken = true;

    // signal a waiting "recv" call if there is any data available
    std::unique_lock<std::mutex> lock(m_RecvDataLock);
    if (m_bSynRecving)
        m_RecvDataCond.notify_all();

    return success();
}

void CUDT::close()
{
    if (!m_bOpened)
        return;

    if (0 != m_Linger.l_onoff)
    {
        const auto entertime = CTimer::getTime();

        while (!m_bBroken && m_bConnected && (m_pSndBuffer->getCurrBufSize() > 0) &&
            (CTimer::getTime() - entertime < std::chrono::seconds(m_Linger.l_linger)))
        {
            // linger has been checked by previous close() call and has expired
            if (m_ullLingerExpiration >= entertime)
                break;

            if (!m_bSynSending)
            {
                // if this socket enables asynchronous sending, return immediately and let GC to close it later
                if (std::chrono::microseconds::zero() == m_ullLingerExpiration)
                    m_ullLingerExpiration = entertime + std::chrono::seconds(m_Linger.l_linger);

                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // remove this socket from the snd queue
    if (m_bConnected)
        sndQueue().sndUList().remove(this);

    // trigger any pending IO events.
    s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_ERR, true);

    // then remove itself from all epoll monitoring
    for (set<int>::iterator i = m_sPollID.begin(); i != m_sPollID.end(); ++i)
        s_UDTUnited->m_EPoll.remove_usock(*i, m_SocketId);

    if (!m_bOpened)
        return;

    // Inform the threads handler to stop.
    setIsClosing(true);

    // Signal the sender and recver if they are waiting for data.
    releaseSynch();

    m_bListening = false;

    m_synPacketHandler = nullptr;

    if (m_multiplexer)
        m_multiplexer->recvQueue().removeConnector(m_SocketId);

    std::lock_guard<std::mutex> cg(m_ConnectionLock);

    if (m_bConnected)
    {
        if (!m_bShutdown)
            sendCtrl(ControlPacketType::Shutdown);

        m_congestionControl.close();

        // Store current connection information.
        CInfoBlock ib;
        ib.m_iIPversion = m_iIPversion;
        CInfoBlock::convert(m_pPeerAddr, ib.m_piIP);
        ib.m_iRTT = m_iRTT;
        ib.m_iBandwidth = m_iBandwidth;
        m_pCache->put(ib.getKey(), ib);

        m_bConnected = false;
    }

    // waiting all send and recv calls to stop
    std::scoped_lock lock(m_SendLock, m_RecvLock);

    // CLOSED.
    m_bOpened = false;
}

Result<int> CUDT::send(const char* data, int len)
{
    if (UDT_DGRAM == m_iSockType)
        return Error(OsErrorCode::notConnected);

    if (!m_bConnected)
        return Error(OsErrorCode::notConnected);
    else if (m_bBroken || isClosing())
        return Error(OsErrorCode::connectionReset);

    if (len <= 0)
        return success(0);

    std::lock_guard<std::mutex> sendguard(m_SendLock);

    if (m_pSndBuffer->getCurrBufSize() == 0)
    {
        // delay the EXP timer to avoid mis-fired timeout
        m_ullLastRspTime = CTimer::getTime();
    }

    if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
    {
        if (!m_bSynSending)
        {
            return Error(OsErrorCode::wouldBlock);
        }
        else
        {
            // wait here during a blocking sending
            std::unique_lock<std::mutex> lock(m_SendBlockLock);
            if (m_sendTimeout < std::chrono::milliseconds::zero())
            {
                while (!m_bBroken && m_bConnected && !isClosing() &&
                    (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth)
                {
                    m_SendBlockCond.wait(lock);
                }
            }
            else
            {
                const auto exptime = CTimer::getTime() + m_sendTimeout;
                for (auto currentTime = CTimer::getTime();
                    !m_bBroken && m_bConnected && !isClosing() &&
                        (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) &&
                        m_bPeerHealth && (currentTime < exptime);
                    currentTime = CTimer::getTime())
                {
                    m_SendBlockCond.wait_for(lock, std::chrono::microseconds(exptime - currentTime));
                }
            }
            lock.unlock();

            // check the connection status
            if (!m_bConnected)
            {
                return Error(OsErrorCode::notConnected);
            }
            else if (m_bBroken || isClosing())
            {
                return Error(OsErrorCode::connectionReset);
            }
            else if (!m_bPeerHealth)
            {
                m_bPeerHealth = true;
                return Error(OsErrorCode::notConnected);
            }
        }
    }

    if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
    {
        if (m_sendTimeout >= std::chrono::milliseconds::zero())
            return Error(OsErrorCode::timedOut);

        return success(0);
    }

    int size = (m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize;
    if (size > len)
        size = len;

    // record total time used for sending
    if (0 == m_pSndBuffer->getCurrBufSize())
        m_llSndDurationCounter = CTimer::getTime();

    // insert the user buffer into the sening list
    m_pSndBuffer->addBuffer(data, size);

    // insert this socket to snd list if it is not on the list yet
    sndQueue().sndUList().update(shared_from_this(), false);

    if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
    {
        // write is not available any more
        s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_OUT, false);
    }

    return success(size);
}

Result<int> CUDT::recv(char* data, int len)
{
    if (UDT_DGRAM == m_iSockType)
        return Error(OsErrorCode::notConnected);

    if (!m_bConnected)
        return Error(OsErrorCode::notConnected);
    else if ((m_bBroken || isClosing()) && (0 == m_pRcvBuffer->getRcvDataSize()))
        return Error(OsErrorCode::connectionReset);

    if (len <= 0)
        return success(0);

    std::lock_guard<std::mutex> recvguard(m_RecvLock);

    if (0 == m_pRcvBuffer->getRcvDataSize())
    {
        if (!m_bSynRecving)
        {
            return Error(OsErrorCode::wouldBlock);
        }
        else
        {
            std::unique_lock<std::mutex> lock(m_RecvDataLock);
            if (m_recvTimeout < std::chrono::milliseconds::zero())
            {
                while (!m_bBroken && m_bConnected && !isClosing() && (0 == m_pRcvBuffer->getRcvDataSize()))
                    m_RecvDataCond.wait(lock);
            }
            else
            {
                const auto exptime = CTimer::getTime() + m_recvTimeout;
                for (auto currentTime = CTimer::getTime();
                    !m_bBroken && m_bConnected && !isClosing() &&
                        (0 == m_pRcvBuffer->getRcvDataSize()) && currentTime < exptime;
                    currentTime = CTimer::getTime())
                {
                    m_RecvDataCond.wait_for(lock, std::chrono::microseconds(exptime - currentTime));
                }
            }
        }
    }

    if (!m_bConnected)
        return Error(OsErrorCode::notConnected);
    else if ((m_bBroken || isClosing()) && (0 == m_pRcvBuffer->getRcvDataSize()))
        return Error(OsErrorCode::connectionReset);

    int res = m_pRcvBuffer->readBuffer(data, len);

    if (m_pRcvBuffer->getRcvDataSize() <= 0)
    {
        // read is not available any more
        s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_IN, false);
    }

    if ((res <= 0) && (m_recvTimeout >= std::chrono::milliseconds::zero()))
        return Error(OsErrorCode::timedOut);

    return success(res);
}

Result<int> CUDT::sendmsg(const char* data, int len, std::chrono::milliseconds ttl, bool inorder)
{
    if (UDT_STREAM == m_iSockType)
        return Error(OsErrorCode::notConnected);

    if (!m_bConnected)
        return Error(OsErrorCode::notConnected);
    else if (m_bBroken || isClosing())
        return Error(OsErrorCode::connectionReset);

    if (len <= 0)
        return success(0);

    if (len > m_iSndBufSize * m_iPayloadSize)
        return Error(OsErrorCode::messageTooLarge);

    std::lock_guard<std::mutex> sendguard(m_SendLock);

    if (m_pSndBuffer->getCurrBufSize() == 0)
    {
        // delay the EXP timer to avoid mis-fired timeout
        m_ullLastRspTime = CTimer::getTime();
    }

    if ((m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize < len)
    {
        if (!m_bSynSending)
        {
            return Error(OsErrorCode::wouldBlock);
        }
        else
        {
            // wait here during a blocking sending
            std::unique_lock<std::mutex> lock(m_SendBlockLock);
            if (m_sendTimeout < std::chrono::milliseconds::zero())
            {
                while (!m_bBroken && m_bConnected && !isClosing() &&
                    ((m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize < len))
                {
                    m_SendBlockCond.wait(lock);
                }
            }
            else
            {
                const auto exptime = CTimer::getTime() + m_sendTimeout * 1000ULL;
                for (auto currentTime = CTimer::getTime();
                    !m_bBroken && m_bConnected && !isClosing() &&
                        ((m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize < len) &&
                        (currentTime < exptime);
                    currentTime = CTimer::getTime())
                {
                    m_SendBlockCond.wait_for(lock, std::chrono::microseconds(exptime - currentTime));
                }
            }
            lock.unlock();

            // check the connection status
            if (!m_bConnected)
                return Error(OsErrorCode::notConnected);
            else if (m_bBroken || isClosing())
                return Error(OsErrorCode::connectionReset);
        }
    }

    if ((m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize < len)
    {
        if (m_sendTimeout >= std::chrono::milliseconds::zero())
            return Error(OsErrorCode::timedOut);

        return success(0);
    }

    // record total time used for sending
    if (0 == m_pSndBuffer->getCurrBufSize())
        m_llSndDurationCounter = CTimer::getTime();

    // insert the user buffer into the sening list
    m_pSndBuffer->addBuffer(data, len, ttl, inorder);

    // insert this socket to the snd list if it is not on the list yet
    sndQueue().sndUList().update(shared_from_this(), false);

    if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
    {
        // write is not available any more
        s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_OUT, false);
    }

    return success(len);
}

Result<int> CUDT::recvmsg(char* data, int len)
{
    if (UDT_STREAM == m_iSockType)
        return Error(OsErrorCode::notConnected);

    if (!m_bConnected)
        return Error(OsErrorCode::notConnected);

    if (len <= 0)
        return success(0);

    std::lock_guard<std::mutex> recvguard(m_RecvLock);

    if (m_bBroken || isClosing())
    {
        int res = m_pRcvBuffer->readMsg(data, len);

        if (m_pRcvBuffer->getRcvMsgNum() <= 0)
        {
            // read is not available any more
            s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_IN, false);
        }

        if (0 == res)
            return Error(OsErrorCode::connectionReset);
        else
            return success(res);
    }

    if (!m_bSynRecving)
    {
        int res = m_pRcvBuffer->readMsg(data, len);
        if (0 == res)
            return Error(OsErrorCode::wouldBlock);
        else
            return success(res);
    }

    int res = 0;
    bool timeout = false;

    do
    {
        std::unique_lock<std::mutex> lock(m_RecvDataLock);
        if (m_recvTimeout < std::chrono::milliseconds::zero())
        {
            while (!m_bBroken && m_bConnected && !isClosing() &&
                (0 == (res = m_pRcvBuffer->readMsg(data, len))))
            {
                m_RecvDataCond.wait(lock);
            }
        }
        else
        {
            if (m_RecvDataCond.wait_for(lock, std::chrono::milliseconds(m_recvTimeout)) ==
                std::cv_status::timeout)
            {
                timeout = true;
            }

            res = m_pRcvBuffer->readMsg(data, len);
        }
        lock.unlock();

        if (!m_bConnected)
            return Error(OsErrorCode::notConnected);
        else if (m_bBroken || isClosing())
            return Error(OsErrorCode::connectionReset);
    } while ((0 == res) && !timeout);

    if (m_pRcvBuffer->getRcvMsgNum() <= 0)
    {
        // read is not available any more
        s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_IN, false);
    }

    if ((res <= 0) && (m_recvTimeout >= std::chrono::milliseconds::zero()))
        return Error(OsErrorCode::timedOut);

    return success(res);
}

Result<int64_t> CUDT::sendfile(fstream& ifs, int64_t& offset, int64_t size, int block)
{
    if (UDT_DGRAM == m_iSockType)
        return Error(OsErrorCode::notConnected);

    if (!m_bConnected)
        return Error(OsErrorCode::notConnected);
    else if (m_bBroken || isClosing())
        return Error(OsErrorCode::connectionReset);

    if (size <= 0)
        return success(int64_t(0));

    std::lock_guard<std::mutex> sendguard(m_SendLock);

    if (m_pSndBuffer->getCurrBufSize() == 0)
    {
        // delay the EXP timer to avoid mis-fired timeout
        m_ullLastRspTime = CTimer::getTime();
    }

    int64_t tosend = size;
    int unitsize;

    // positioning...
    ifs.seekg((streamoff)offset);

    // sending block by block
    while (tosend > 0)
    {
        if (ifs.fail())
            return Error(OsErrorCode::io);

        if (ifs.eof())
            break;

        unitsize = int((tosend >= block) ? block : tosend);

        std::unique_lock<std::mutex> sendBlockLock(m_SendBlockLock);
        while (!m_bBroken && m_bConnected && !isClosing() &&
            (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth)
        {
            m_SendBlockCond.wait(sendBlockLock);
        }
        sendBlockLock.unlock();

        if (!m_bConnected)
        {
            return Error(OsErrorCode::notConnected);
        }
        else if (m_bBroken || isClosing())
        {
            return Error(OsErrorCode::connectionReset);
        }
        else if (!m_bPeerHealth)
        {
            // reset peer health status, once this error returns, the app should handle the situation at the peer side
            m_bPeerHealth = true;
            return Error(OsErrorCode::connectionReset);
        }

        // record total time used for sending
        if (0 == m_pSndBuffer->getCurrBufSize())
            m_llSndDurationCounter = CTimer::getTime();

        int64_t sentsize = m_pSndBuffer->addBufferFromFile(ifs, unitsize);

        if (sentsize > 0)
        {
            tosend -= sentsize;
            offset += sentsize;
        }

        // insert this socket to snd list if it is not on the list yet
        sndQueue().sndUList().update(shared_from_this(), false);
    }

    if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
    {
        // write is not available any more
        s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_OUT, false);
    }

    return success(size - tosend);
}

Result<int64_t> CUDT::recvfile(fstream& ofs, int64_t& offset, int64_t size, int block)
{
    if (UDT_DGRAM == m_iSockType)
        return Error(OsErrorCode::notConnected);

    if (!m_bConnected)
        return Error(OsErrorCode::notConnected);
    else if ((m_bBroken || isClosing()) && (0 == m_pRcvBuffer->getRcvDataSize()))
        return Error(OsErrorCode::connectionReset);

    if (size <= 0)
        return success(int64_t(0));

    std::lock_guard<std::mutex> recvguard(m_RecvLock);

    int64_t torecv = size;
    int unitsize = block;
    int recvsize;

    ofs.seekp((streamoff)offset);

    // receiving... "recvfile" is always blocking
    while (torecv > 0)
    {
        if (ofs.fail())
        {
            // Sending the sender a signal so it will not be blocked forever.
            static constexpr int fileIoErrorCode = 4000;
            int32_t err_code = fileIoErrorCode;
            sendCtrl(ControlPacketType::RemotePeerFailure, &err_code);

            return Error(OsErrorCode::io);
        }

        {
            std::unique_lock<std::mutex> lock(m_RecvDataLock);
            while (!m_bBroken && m_bConnected && !isClosing() && (0 == m_pRcvBuffer->getRcvDataSize()))
                m_RecvDataCond.wait(lock);
        }

        if (!m_bConnected)
            return Error(OsErrorCode::notConnected);
        else if ((m_bBroken || isClosing()) && (0 == m_pRcvBuffer->getRcvDataSize()))
            return Error(OsErrorCode::connectionReset);

        unitsize = int((torecv >= block) ? block : torecv);
        recvsize = m_pRcvBuffer->readBufferToFile(ofs, unitsize);

        if (recvsize > 0)
        {
            torecv -= recvsize;
            offset += recvsize;
        }
    }

    if (m_pRcvBuffer->getRcvDataSize() <= 0)
    {
        // read is not available any more
        s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_IN, false);
    }

    return success(size - torecv);
}

Result<> CUDT::sample(CPerfMon* perf, bool clear)
{
    if (!m_bConnected)
        return Error(OsErrorCode::notConnected);
    else if (m_bBroken || isClosing())
        return Error(OsErrorCode::connectionReset);

    const auto currtime = CTimer::getTime();
    perf->msTimeStamp = std::chrono::floor<std::chrono::milliseconds>(currtime - m_StartTime);

    perf->pktSent = m_llTraceSent;
    perf->pktRecv = m_llTraceRecv;
    perf->pktSndLoss = m_iTraceSndLoss;
    perf->pktRcvLoss = m_iTraceRcvLoss;
    perf->pktRetrans = m_iTraceRetrans;
    perf->pktSentACK = m_iSentACK;
    perf->pktRecvACK = m_iRecvACK;
    perf->pktSentNAK = m_iSentNAK;
    perf->pktRecvNAK = m_iRecvNAK;
    perf->usSndDuration = m_llSndDuration;

    perf->pktSentTotal = m_llSentTotal;
    perf->pktRecvTotal = m_llRecvTotal;
    perf->pktSndLossTotal = m_iSndLossTotal;
    perf->pktRcvLossTotal = m_iRcvLossTotal;
    perf->pktRetransTotal = m_iRetransTotal;
    perf->pktSentACKTotal = m_iSentACKTotal;
    perf->pktRecvACKTotal = m_iRecvACKTotal;
    perf->pktSentNAKTotal = m_iSentNAKTotal;
    perf->pktRecvNAKTotal = m_iRecvNAKTotal;
    perf->usSndDurationTotal = m_llSndDurationTotal;

    double interval = double((currtime - m_LastSampleTime).count());

    perf->mbpsSendRate = double(m_llTraceSent) * m_iPayloadSize * 8.0 / interval;
    perf->mbpsRecvRate = double(m_llTraceRecv) * m_iPayloadSize * 8.0 / interval;

    perf->usPktSndPeriod = m_ullInterval.count() / double(m_ullCPUFrequency);
    perf->pktFlowWindow = m_iFlowWindowSize;
    perf->pktCongestionWindow = (int)m_dCongestionWindow;
    perf->pktFlightSize = CSeqNo::seqlen(m_iSndLastAck, CSeqNo::incseq(m_iSndCurrSeqNo)) - 1;
    perf->rtt = m_iRTT;
    perf->mbpsBandwidth = m_iBandwidth * m_iPayloadSize * 8.0 / 1000000.0;

    std::unique_lock<std::mutex> connectionLock(m_ConnectionLock, std::try_to_lock);
    if (connectionLock.owns_lock())
    {
        perf->byteAvailSndBuf = (NULL == m_pSndBuffer) ? 0 : (m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iMSS;
        perf->byteAvailRcvBuf = (NULL == m_pRcvBuffer) ? 0 : m_pRcvBuffer->getAvailBufSize() * m_iMSS;

        connectionLock.unlock();
    }
    else
    {
        perf->byteAvailSndBuf = 0;
        perf->byteAvailRcvBuf = 0;
    }

    if (clear)
    {
        m_llTraceSent = m_llTraceRecv = m_iTraceSndLoss = m_iTraceRcvLoss = m_iTraceRetrans = m_iSentACK = m_iRecvACK = m_iSentNAK = m_iRecvNAK = 0;
        m_llSndDuration = std::chrono::microseconds::zero();
        m_LastSampleTime = currtime;
    }

    return success();
}

void CUDT::CCUpdate()
{
    m_ullInterval = std::chrono::microseconds(
        (uint64_t)(m_congestionControl.m_dPktSndPeriod * m_ullCPUFrequency));
    m_dCongestionWindow = m_congestionControl.m_dCWndSize;

    if (m_llMaxBW <= 0)
        return;

    const auto minSP = std::chrono::microseconds(
        (uint64_t)(1000000.0 / (double(m_llMaxBW) / m_iMSS) * m_ullCPUFrequency));
    if (m_ullInterval < minSP)
        m_ullInterval = minSP;
}

void CUDT::releaseSynch()
{
    // TODO: #akolesnikov Truly crazy function.

    {
        std::unique_lock<std::mutex> lock(m_SendBlockLock);
        m_SendBlockCond.notify_all();
    }

    {
        std::lock_guard<std::mutex> sendguard(m_SendLock);
    }

    {
        std::unique_lock<std::mutex> lock(m_RecvDataLock);
        m_RecvDataCond.notify_all();
    }

    m_RecvLock.lock();
    m_RecvLock.unlock();
}

void CUDT::sendCtrl(ControlPacketType pkttype, void* lparam, void* rparam, int size)
{
    switch (pkttype)
    {
        case ControlPacketType::Acknowledgement: //010 - Acknowledgement
        {
            CPacket ctrlpkt;

            int32_t ack;

            // If there is no loss, the ACK is the current largest sequence number plus 1;
            // Otherwise it is the smallest sequence number in the receiver loss list.
            if (0 == m_pRcvLossList->getLossLength())
                ack = CSeqNo::incseq(m_iRcvCurrSeqNo);
            else
                ack = m_pRcvLossList->getFirstLostSeq();

            if (ack == m_iRcvLastAckAck)
                break;

            // send out a lite ACK
            // to save time on buffer processing and bandwidth/AS measurement, a lite ACK only feeds back an ACK number
            if (4 == size)
            {
                ctrlpkt.pack(pkttype, NULL, size);
                memcpy(ctrlpkt.payload().data(), &ack, size);
                ctrlpkt.m_iID = m_PeerID;
                sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

                break;
            }

            const auto currtime = CTimer::getTime();

            // There are new received packets to acknowledge, update related information.
            if (CSeqNo::seqcmp(ack, m_iRcvLastAck) > 0)
            {
                int acksize = CSeqNo::seqoff(m_iRcvLastAck, ack);

                m_iRcvLastAck = ack;

                m_pRcvBuffer->ackData(acksize);

                // signal a waiting "recv" call if there is any data available
                {
                    std::unique_lock<std::mutex> lock(m_RecvDataLock);
                    if (m_bSynRecving)
                        m_RecvDataCond.notify_all();
                }

                // acknowledge any waiting epolls to read
                s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_IN, true);
            }
            else if (ack == m_iRcvLastAck)
            {
                if ((currtime - m_ullLastAckTime) < ((m_iRTT + 4 * m_iRTTVar) * m_ullCPUFrequency))
                    break;
            }
            else
            {
                break;
            }

            // Send out the ACK only if has not been received by the sender before
            if (CSeqNo::seqcmp(m_iRcvLastAck, m_iRcvLastAckAck) > 0)
            {
                int32_t data[6];

                m_iAckSeqNo = CAckNo::incack(m_iAckSeqNo);
                data[0] = m_iRcvLastAck;
                data[1] = m_iRTT.count();
                data[2] = m_iRTTVar.count();
                data[3] = m_pRcvBuffer->getAvailBufSize();
                // a minimum flow window of 2 is used, even if buffer is full, to break potential deadlock
                if (data[3] < 2)
                    data[3] = 2;

                if (currtime - m_ullLastAckTime > m_ullSYNInt)
                {
                    data[4] = m_pRcvTimeWindow->getPktRcvSpeed();
                    data[5] = m_pRcvTimeWindow->getBandwidth();
                    ctrlpkt.pack(pkttype, &m_iAckSeqNo, 24);
                    memcpy(ctrlpkt.payload().data(), data, 24);

                    m_ullLastAckTime = CTimer::getTime();
                }
                else
                {
                    ctrlpkt.pack(pkttype, &m_iAckSeqNo, 16);
                    memcpy(ctrlpkt.payload().data(), data, 16);
                }

                ctrlpkt.m_iID = m_PeerID;
                sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

                m_pACKWindow->store(m_iAckSeqNo, m_iRcvLastAck);

                ++m_iSentACK;
                ++m_iSentACKTotal;
            }

            break;
        }

        case ControlPacketType::AcknowledgementOfAcknowledgement: //110 - Acknowledgement of Acknowledgement
        {
            CPacket ctrlpkt;
            ctrlpkt.pack(pkttype, lparam);
            ctrlpkt.m_iID = m_PeerID;
            sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

            break;
        }

        case ControlPacketType::LossReport: //011 - Loss Report
        {
            CPacket ctrlpkt;

            if (NULL != rparam)
            {
                if (1 == size)
                {
                    // only 1 loss packet
                    ctrlpkt.pack(pkttype, NULL, 4);
                    ctrlpkt.payload().assign((const char*)((int32_t*)rparam + 1), 4);
                }
                else
                {
                    // more than 1 loss packets
                    ctrlpkt.pack(pkttype, NULL, 8);
                    ctrlpkt.payload().assign((const char*) rparam, 8);
                }

                ctrlpkt.m_iID = m_PeerID;
                sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

                ++m_iSentNAK;
                ++m_iSentNAKTotal;
            }
            else if (m_pRcvLossList->getLossLength() > 0)
            {
                // this is periodically NAK report; make sure NAK cannot be sent back too often

                // read loss list from the local receiver loss list
                ctrlpkt.pack(pkttype, NULL, (m_iPayloadSize / 4) * sizeof(int32_t));

                int losslen = 0;
                m_pRcvLossList->getLossArray(
                    (int32_t*) ctrlpkt.payload().data(), losslen, m_iPayloadSize / 4);

                if (losslen > 0)
                {
                    ctrlpkt.payload().resize(losslen * sizeof(int32_t));

                    ctrlpkt.m_iID = m_PeerID;
                    sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

                    ++m_iSentNAK;
                    ++m_iSentNAKTotal;
                }
            }

            // update next NAK time, which should wait enough time for the retansmission, but not too long
            m_ullNAKInt = (m_iRTT + 4 * m_iRTTVar) * m_ullCPUFrequency;
            int rcv_speed = m_pRcvTimeWindow->getPktRcvSpeed();
            if (rcv_speed > 0)
            {
                m_ullNAKInt += std::chrono::microseconds(
                    (m_pRcvLossList->getLossLength() * 1000000ULL / rcv_speed) * m_ullCPUFrequency);
            }
            if (m_ullNAKInt < m_ullMinNakInt)
                m_ullNAKInt = m_ullMinNakInt;

            break;
        }

        case ControlPacketType::DelayWarning: //100 - Congestion Warning
        {
            CPacket ctrlpkt;
            ctrlpkt.pack(pkttype);
            ctrlpkt.m_iID = m_PeerID;
            sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

            CTimer::rdtsc(m_ullLastWarningTime);

            break;
        }

        case ControlPacketType::KeepAlive: //001 - Keep-alive
        {
            CPacket ctrlpkt;
            ctrlpkt.pack(pkttype);
            ctrlpkt.m_iID = m_PeerID;
            sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

            break;
        }

        case ControlPacketType::Handshake: //000 - Handshake
        {
            CPacket ctrlpkt;
            ctrlpkt.pack(pkttype, NULL, sizeof(CHandShake));
            ctrlpkt.payload().assign((const char*) rparam, sizeof(CHandShake));
            ctrlpkt.m_iID = m_PeerID;
            sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

            break;
        }

        case ControlPacketType::Shutdown: //101 - Shutdown
        {
            CPacket ctrlpkt;
            ctrlpkt.pack(pkttype);
            ctrlpkt.m_iID = m_PeerID;
            sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

            break;
        }

        case ControlPacketType::MsgDropRequest: //111 - Msg drop request
        {
            CPacket ctrlpkt;
            ctrlpkt.pack(pkttype, lparam, 8);
            ctrlpkt.payload().assign((const char*) rparam, 8);
            ctrlpkt.m_iID = m_PeerID;
            sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

            break;
        }

        case ControlPacketType::RemotePeerFailure: //1000 - acknowledge the peer side a special error
        {
            CPacket ctrlpkt;
            ctrlpkt.pack(pkttype, lparam);
            ctrlpkt.m_iID = m_PeerID;
            sndQueue().sendto(m_pPeerAddr, std::move(ctrlpkt));

            break;
        }

        case ControlPacketType::Reserved: //0x7FFF - Resevered for future use
            break;

        default:
            break;
    }
}

void CUDT::processCtrl(CPacket& ctrlpkt)
{
    // Just heard from the peer, reset the expiration count.
    m_iEXPCount = 1;

    const auto currtime = CTimer::getTime();
    m_ullLastRspTime = currtime;

    switch (ctrlpkt.getType())
    {
        case ControlPacketType::Acknowledgement: //010 - Acknowledgement
        {
            int32_t ack;

            // process a lite ACK
            if (4 == ctrlpkt.getLength())
            {
                ack = *(int32_t *)ctrlpkt.payload().data();
                if (CSeqNo::seqcmp(ack, m_iSndLastAck) >= 0)
                {
                    m_iFlowWindowSize -= CSeqNo::seqoff(m_iSndLastAck, ack);
                    m_iSndLastAck = ack;
                }

                break;
            }

            // read ACK seq. no.
            ack = ctrlpkt.getAckSeqNo();

            // send ACK acknowledgement
            // number of ACK2 can be much less than number of ACK
            const auto now = CTimer::getTime();
            if ((currtime - m_ullSndLastAck2Time > m_iSYNInterval) || (ack == m_iSndLastAck2))
            {
                sendCtrl(ControlPacketType::AcknowledgementOfAcknowledgement, &ack);
                m_iSndLastAck2 = ack;
                m_ullSndLastAck2Time = now;
            }

            // Got data ACK
            ack = *(int32_t *)ctrlpkt.payload().data();

            // check the validation of the ack
            if (CSeqNo::seqcmp(ack, CSeqNo::incseq(m_iSndCurrSeqNo)) > 0)
            {
                //this should not happen: attack or bug
                setBroken(true);
                m_iBrokenCounter = 0;
                break;
            }

            if (CSeqNo::seqcmp(ack, m_iSndLastAck) >= 0)
            {
                // Update Flow Window Size, must update before and together with m_iSndLastAck
                m_iFlowWindowSize = *((int32_t *)ctrlpkt.payload().data() + 3);
                m_iSndLastAck = ack;
            }

            // protect packet retransmission
            std::unique_lock<std::mutex> ackLocker(m_AckLock);

            int offset = CSeqNo::seqoff(m_iSndLastDataAck, ack);
            if (offset <= 0)
            {
                // discard it if it is a repeated ACK
                break;
            }

            // acknowledge the sending buffer
            m_pSndBuffer->ackData(offset);

            // record total time used for sending
            m_llSndDuration += currtime - m_llSndDurationCounter;
            m_llSndDurationTotal += currtime - m_llSndDurationCounter;
            m_llSndDurationCounter = currtime;

            // update sending variables
            m_iSndLastDataAck = ack;
            m_pSndLossList->remove(CSeqNo::decseq(m_iSndLastDataAck));

            ackLocker.unlock();

            {
                std::unique_lock<std::mutex> sendBlockLock(m_SendBlockLock);
                if (m_bSynSending)
                    m_SendBlockCond.notify_all();
            }

            // acknowledde any waiting epolls to write
            s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_OUT, true);

            // insert this socket to snd list if it is not on the list yet
            sndQueue().sndUList().update(shared_from_this(), false);

            // Update RTT
            //m_iRTT = *((int32_t *)ctrlpkt.m_pcData + 1);
            //m_iRTTVar = *((int32_t *)ctrlpkt.m_pcData + 2);
            int rtt = *((int32_t *)ctrlpkt.payload().data() + 1);
            m_iRTTVar = std::chrono::microseconds((m_iRTTVar.count() * 3 + abs(rtt - m_iRTT.count())) >> 2);
            m_iRTT = std::chrono::microseconds((m_iRTT.count() * 7 + rtt) >> 3);

            m_congestionControl.setRTT(m_iRTT);

            if (ctrlpkt.getLength() > 16)
            {
                // Update Estimated Bandwidth and packet delivery rate
                if (*((int32_t *)ctrlpkt.payload().data() + 4) > 0)
                    m_iDeliveryRate = (m_iDeliveryRate * 7 + *((int32_t *)ctrlpkt.payload().data() + 4)) >> 3;

                if (*((int32_t *)ctrlpkt.payload().data() + 5) > 0)
                    m_iBandwidth = (m_iBandwidth * 7 + *((int32_t *)ctrlpkt.payload().data() + 5)) >> 3;

                m_congestionControl.setRcvRate(m_iDeliveryRate);
                m_congestionControl.setBandwidth(m_iBandwidth);
            }

            m_congestionControl.onACK(ack);
            CCUpdate();

            ++m_iRecvACK;
            ++m_iRecvACKTotal;

            break;
        }

        case ControlPacketType::AcknowledgementOfAcknowledgement: //110 - Acknowledgement of Acknowledgement
        {
            int32_t ack;
            int rtt = -1;

            // update RTT
            rtt = m_pACKWindow->acknowledge(ctrlpkt.getAckSeqNo(), ack);
            if (rtt <= 0)
                break;

            //if increasing delay detected...
            //   sendCtrl(ControlPacketType::DelayWarning);

            // RTT EWMA
            m_iRTTVar = std::chrono::microseconds((m_iRTTVar.count() * 3 + abs(rtt - m_iRTT.count())) >> 2);
            m_iRTT = std::chrono::microseconds((m_iRTT.count() * 7 + rtt) >> 3);

            m_congestionControl.setRTT(m_iRTT);

            // update last ACK that has been received by the sender
            if (CSeqNo::seqcmp(ack, m_iRcvLastAckAck) > 0)
                m_iRcvLastAckAck = ack;

            break;
        }

        case ControlPacketType::LossReport: //011 - Loss Report
        {
            int32_t* losslist = (int32_t *)(ctrlpkt.payload().data());

            m_congestionControl.onLoss(losslist, ctrlpkt.getLength() / 4);
            CCUpdate();

            bool secure = true;

            // decode loss list message and insert loss into the sender loss list
            for (int i = 0, n = (int)(ctrlpkt.getLength() / 4); i < n; ++i)
            {
                if (0 != (losslist[i] & 0x80000000))
                {
                    if ((CSeqNo::seqcmp(losslist[i] & 0x7FFFFFFF, losslist[i + 1]) > 0) || (CSeqNo::seqcmp(losslist[i + 1], m_iSndCurrSeqNo) > 0))
                    {
                        // seq_a must not be greater than seq_b; seq_b must not be greater than the most recent sent seq
                        secure = false;
                        break;
                    }

                    int num = 0;
                    if (CSeqNo::seqcmp(losslist[i] & 0x7FFFFFFF, m_iSndLastAck) >= 0)
                        num = m_pSndLossList->insert(losslist[i] & 0x7FFFFFFF, losslist[i + 1]);
                    else if (CSeqNo::seqcmp(losslist[i + 1], m_iSndLastAck) >= 0)
                        num = m_pSndLossList->insert(m_iSndLastAck, losslist[i + 1]);

                    m_iTraceSndLoss += num;
                    m_iSndLossTotal += num;

                    ++i;
                }
                else if (CSeqNo::seqcmp(losslist[i], m_iSndLastAck) >= 0)
                {
                    if (CSeqNo::seqcmp(losslist[i], m_iSndCurrSeqNo) > 0)
                    {
                        //seq_a must not be greater than the most recent sent seq
                        secure = false;
                        break;
                    }

                    int num = m_pSndLossList->insert(losslist[i], losslist[i]);

                    m_iTraceSndLoss += num;
                    m_iSndLossTotal += num;
                }
            }

            if (!secure)
            {
                //this should not happen: attack or bug
                setBroken(true);
                m_iBrokenCounter = 0;
                break;
            }

            // the lost packet (retransmission) should be sent out immediately
            sndQueue().sndUList().update(shared_from_this());

            ++m_iRecvNAK;
            ++m_iRecvNAKTotal;

            break;
        }

        case ControlPacketType::DelayWarning:
            // One way packet delay is increasing, so decrease the sending rate
            m_ullInterval = std::chrono::microseconds((uint64_t)ceil(m_ullInterval.count() * 1.125));
            m_iLastDecSeq = m_iSndCurrSeqNo;

            break;

        case ControlPacketType::KeepAlive:
            // The only purpose of keep-alive packet is to tell that the peer is still alive
            // nothing needs to be done.

            break;

        case ControlPacketType::Handshake:
        {
            CHandShake req;
            req.deserialize(ctrlpkt.payload().data(), ctrlpkt.getLength());
            if ((req.m_iReqType > 0) || (m_bRendezvous && (req.m_iReqType != -2)))
            {
                // The peer side has not received the handshake message, so it keeps querying
                // resend the handshake packet

                CHandShake initdata;
                initdata.m_iISN = m_iISN;
                initdata.m_iMSS = m_iMSS;
                initdata.m_iFlightFlagSize = m_iFlightFlagSize;
                initdata.m_iReqType = (!m_bRendezvous) ? -1 : -2;
                initdata.m_iID = m_SocketId;

                char* hs = new char[m_iPayloadSize];
                int hs_size = m_iPayloadSize;
                initdata.serialize(hs, hs_size);
                sendCtrl(ControlPacketType::Handshake, NULL, hs, hs_size);
                delete[] hs;
            }

            break;
        }

        case ControlPacketType::Shutdown:
            m_bShutdown = true;
            setIsClosing(true);
            setBroken(true);
            m_iBrokenCounter = 60;

            //performing ACK on existing data to provide data already-in-buffer to recv
            checkTimers(true);

            // Signal the sender and recver if they are waiting for data.
            releaseSynch();
            CTimer::triggerEvent();

            break;

        case ControlPacketType::MsgDropRequest:
            m_pRcvBuffer->dropMsg(ctrlpkt.getMsgSeq());
            m_pRcvLossList->remove(*(int32_t*)ctrlpkt.payload().data(), *(int32_t*)(ctrlpkt.payload().data() + 4));

            // move forward with current recv seq no.
            if ((CSeqNo::seqcmp(*(int32_t*)ctrlpkt.payload().data(), CSeqNo::incseq(m_iRcvCurrSeqNo)) <= 0)
                && (CSeqNo::seqcmp(*(int32_t*)(ctrlpkt.payload().data() + 4), m_iRcvCurrSeqNo) > 0))
            {
                m_iRcvCurrSeqNo = *(int32_t*)(ctrlpkt.payload().data() + 4);
            }

            break;

        case ControlPacketType::RemotePeerFailure:
            //int err_type = packet.getAddInfo();

            // currently only this error is signalled from the peer side
            // if recvfile() failes (e.g., due to disk fail), blcoked sendfile/send should return immediately
            // giving the app a chance to fix the issue

            m_bPeerHealth = false;

            break;

        case ControlPacketType::Reserved:
            m_congestionControl.processCustomMsg(&ctrlpkt);
            CCUpdate();

            break;

        default:
            break;
    }
}

int CUDT::packData(CPacket& packet, std::chrono::microseconds& ts)
{
    Buffer payload;
    bool probe = false;

    const auto entertime = CTimer::getTime();

    if ((std::chrono::microseconds::zero() != m_ullTargetTime) && (entertime > m_ullTargetTime))
        m_ullTimeDiff += entertime - m_ullTargetTime;

    // Loss retransmission always has higher priority.
    if ((packet.m_iSeqNo = m_pSndLossList->getLostSeq()) >= 0)
    {
        // protect m_iSndLastDataAck from updating by ACK processing
        std::lock_guard<std::mutex> ackLocker(m_AckLock);

        int offset = CSeqNo::seqoff(m_iSndLastDataAck, packet.m_iSeqNo);
        if (offset < 0)
            return 0;

        int msglen;

        auto data = m_pSndBuffer->readData(offset, packet.m_iMsgNo, msglen);
        if (!data)
        {
            int32_t seqpair[2];
            seqpair[0] = packet.m_iSeqNo;
            seqpair[1] = CSeqNo::incseq(seqpair[0], msglen);
            sendCtrl(ControlPacketType::MsgDropRequest, &packet.m_iMsgNo, seqpair, 8);

            // only one msg drop request is necessary
            m_pSndLossList->remove(seqpair[1]);

            // skip all dropped packets
            if (CSeqNo::seqcmp(m_iSndCurrSeqNo, CSeqNo::incseq(seqpair[1])) < 0)
                m_iSndCurrSeqNo = CSeqNo::incseq(seqpair[1]);

            return 0;
        }
        else if (data->empty())
        {
            return 0;
        }

        payload = std::move(*data);
        ++m_iTraceRetrans;
        ++m_iRetransTotal;
    }
    else
    {
        // If no loss, pack a new packet.

        // check congestion/flow window limit
        int cwnd = (m_iFlowWindowSize < (int)m_dCongestionWindow) ? m_iFlowWindowSize : (int)m_dCongestionWindow;
        if (cwnd >= CSeqNo::seqlen(m_iSndLastAck, CSeqNo::incseq(m_iSndCurrSeqNo)))
        {
            auto data = m_pSndBuffer->readData(packet.m_iMsgNo);
            if (data && !data->empty())
            {
                m_iSndCurrSeqNo = CSeqNo::incseq(m_iSndCurrSeqNo);
                m_congestionControl.setSndCurrSeqNo(m_iSndCurrSeqNo);

                packet.m_iSeqNo = m_iSndCurrSeqNo;

                // every 16 (0xF) packets, a packet pair is sent
                if (0 == (packet.m_iSeqNo & 0xF))
                    probe = true;
            }
            else
            {
                m_ullTargetTime = std::chrono::microseconds::zero();
                m_ullTimeDiff = std::chrono::microseconds::zero();
                ts = std::chrono::microseconds::zero();
                return 0;
            }

            payload = std::move(*data);
        }
        else
        {
            m_ullTargetTime = std::chrono::microseconds::zero();
            m_ullTimeDiff = std::chrono::microseconds::zero();
            ts = std::chrono::microseconds::zero();
            return 0;
        }
    }

    packet.m_iTimeStamp = (int) (CTimer::getTime() - m_StartTime).count();
    packet.m_iID = m_PeerID;

    const auto payloadSize = payload.size();
    packet.setPayload(std::move(payload));

    m_congestionControl.onPktSent(&packet);
    //m_pSndTimeWindow->onPktSent(packet.m_iTimeStamp);

    ++m_llTraceSent;
    ++m_llSentTotal;

    if (probe)
    {
        // sends out probing packet pair
        ts = entertime;
        probe = false;
    }
    else
    {
#ifndef NO_BUSY_WAITING
        ts = entertime + m_ullInterval;
#else
        if (m_ullTimeDiff >= m_ullInterval)
        {
            ts = entertime;
            m_ullTimeDiff -= m_ullInterval;
        }
        else
        {
            ts = entertime + m_ullInterval - m_ullTimeDiff;
            m_ullTimeDiff = std::chrono::microseconds::zero();
        }
#endif
    }

    m_ullTargetTime = ts;

    return payloadSize;
}

Result<> CUDT::processData(Unit unit)
{
    CPacket& packet = unit.packet();

    // Just heard from the peer, reset the expiration count.
    m_iEXPCount = 1;
    const auto currtime = CTimer::getTime();
    m_ullLastRspTime = currtime;

    m_congestionControl.onPktReceived(&packet);
    ++m_iPktCount;
    // update time information
    m_pRcvTimeWindow->onPktArrival();

    // check if it is probing packet pair
    if (0 == (packet.m_iSeqNo & 0xF))
        m_pRcvTimeWindow->probe1Arrival();
    else if (1 == (packet.m_iSeqNo & 0xF))
        m_pRcvTimeWindow->probe2Arrival();

    ++m_llTraceRecv;
    ++m_llRecvTotal;

    int32_t offset = CSeqNo::seqoff(m_iRcvLastAck, packet.m_iSeqNo);
    if ((offset < 0) || (offset >= m_pRcvBuffer->getAvailBufSize()))
        return Error(UDT::ProtocolError::outOfWindowDataReceived);

    if (!m_pRcvBuffer->addData(std::move(unit), offset))
        return Error(UDT::ProtocolError::retransmitReceived);

    // Loss detection.
    if (CSeqNo::seqcmp(packet.m_iSeqNo, CSeqNo::incseq(m_iRcvCurrSeqNo)) > 0)
    {
        // If loss found, insert them to the receiver loss list
        m_pRcvLossList->insert(CSeqNo::incseq(m_iRcvCurrSeqNo), CSeqNo::decseq(packet.m_iSeqNo));

        // pack loss list for NAK
        int32_t lossdata[2];
        lossdata[0] = CSeqNo::incseq(m_iRcvCurrSeqNo) | 0x80000000;
        lossdata[1] = CSeqNo::decseq(packet.m_iSeqNo);

        // Generate loss report immediately.
        sendCtrl(ControlPacketType::LossReport, NULL, lossdata, (CSeqNo::incseq(m_iRcvCurrSeqNo) == CSeqNo::decseq(packet.m_iSeqNo)) ? 1 : 2);

        int loss = CSeqNo::seqlen(m_iRcvCurrSeqNo, packet.m_iSeqNo) - 2;
        m_iTraceRcvLoss += loss;
        m_iRcvLossTotal += loss;
    }

    // This is not a regular fixed size packet...
    // an irregular sized packet usually indicates the end of a message, so send an ACK immediately
    if (packet.getLength() != m_iPayloadSize)
        m_ullNextACKTime = CTimer::getTime();

    // Update the current largest sequence number that has been received.
    // Or it is a retransmitted packet, remove it from receiver loss list.
    if (CSeqNo::seqcmp(packet.m_iSeqNo, m_iRcvCurrSeqNo) > 0)
        m_iRcvCurrSeqNo = packet.m_iSeqNo;
    else
        m_pRcvLossList->remove(packet.m_iSeqNo);

    return success();
}

void CUDT::checkTimers(bool forceAck)
{
    // update CC parameters
    CCUpdate();
    //uint64_t minint = (uint64_t)(m_ullCPUFrequency * m_pSndTimeWindow->getMinPktSndInt() * 0.9);
    //if (m_ullInterval < minint)
    //   m_ullInterval = minint;

    auto currtime = CTimer::getTime();

    if ((currtime > m_ullNextACKTime) ||
        ((m_congestionControl.m_iACKInterval > 0) && (m_congestionControl.m_iACKInterval <= m_iPktCount)) ||
        forceAck)
    {
        // ACK timer expired or ACK interval is reached

        sendCtrl(ControlPacketType::Acknowledgement);
        currtime = CTimer::getTime();
        if (m_congestionControl.m_iACKPeriod > std::chrono::microseconds::zero())
            m_ullNextACKTime = currtime + m_congestionControl.m_iACKPeriod * m_ullCPUFrequency;
        else
            m_ullNextACKTime = currtime + m_ullACKInt;

        m_iPktCount = 0;
        m_iLightACKCount = 1;
    }
    else if (m_iSelfClockInterval * m_iLightACKCount <= m_iPktCount)
    {
        //send a "light" ACK
        sendCtrl(ControlPacketType::Acknowledgement, NULL, NULL, 4);
        ++m_iLightACKCount;
    }

    // we are not sending back repeated NAK anymore and rely on the sender's EXP for retransmission
    //if ((m_pRcvLossList->getLossLength() > 0) && (currtime > m_ullNextNAKTime))
    //{
    //   // NAK timer expired, and there is loss to be reported.
    //   sendCtrl(ControlPacketType::LossReport);
    //
    //   CTimer::rdtsc(currtime);
    //   m_ullNextNAKTime = currtime + m_ullNAKInt;
    //}

    std::chrono::microseconds next_exp_time;
    if (m_congestionControl.m_bUserDefinedRTO)
        next_exp_time = m_ullLastRspTime + m_congestionControl.m_iRTO * m_ullCPUFrequency;
    else
    {
        std::chrono::microseconds exp_int =
            (m_iEXPCount * (m_iRTT + 4 * m_iRTTVar) + m_iSYNInterval) * m_ullCPUFrequency;
        if (exp_int < m_iEXPCount * m_ullMinExpInt)
            exp_int = m_iEXPCount * m_ullMinExpInt;
        next_exp_time = m_ullLastRspTime + exp_int;
    }

    if (currtime > next_exp_time)
    {
        // Haven't receive any information from the peer, is it dead?!
        // timeout: at least 16 expirations and must be greater than 10 seconds
        if ((m_iEXPCount > 16) && (currtime - m_ullLastRspTime > kConnectionBrokenTimeout))
        {
            //
            // Connection is broken.
            // UDT does not signal any information about this instead of to stop quietly.
            // Application will detect this when it calls any UDT methods next time.
            //
            setIsClosing(true);
            setBroken(true);
            m_iBrokenCounter = 30;
            m_bShutdown = false;

            // update snd U list to remove this socket
            sndQueue().sndUList().update(shared_from_this());

            releaseSynch();

            // app can call any UDT API to learn the connection_broken error
            s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, UDT_EPOLL_IN | UDT_EPOLL_OUT | UDT_EPOLL_ERR, true);

            return;
        }

        // sender: Insert all the packets sent after last received acknowledgement into the sender loss list.
        // recver: Send out a keep-alive packet
        if (m_pSndBuffer->getCurrBufSize() > 0)
        {
            if ((CSeqNo::incseq(m_iSndCurrSeqNo) != m_iSndLastAck) && (m_pSndLossList->getLossLength() == 0))
            {
                // resend all unacknowledged packets on timeout, but only if there is no packet in the loss list
                int32_t csn = m_iSndCurrSeqNo;
                int num = m_pSndLossList->insert(m_iSndLastAck, csn);
                m_iTraceSndLoss += num;
                m_iSndLossTotal += num;
            }

            m_congestionControl.onTimeout();
            CCUpdate();

            // immediately restart transmission
            sndQueue().sndUList().update(shared_from_this());
        }
        else
        {
            sendCtrl(ControlPacketType::KeepAlive);
        }

        ++m_iEXPCount;
        // Reset last response time since we just sent a heart-beat.
        m_ullLastRspTime = currtime;
    }
}

void CUDT::addEPoll(const int eid, int eventsToReport)
{
    {
        std::lock_guard<std::mutex> lk(s_UDTUnited->m_EPoll.m_EPollLock);
        m_sPollID.insert(eid);
    }

    if (auto synPacketHandler = m_synPacketHandler)
        synPacketHandler->addEPoll(eid);

    if (m_bConnected && !m_bBroken && !isClosing())
    {
        if (((UDT_STREAM == m_iSockType) && (m_pRcvBuffer->getRcvDataSize() > 0)) ||
            ((UDT_DGRAM == m_iSockType) && (m_pRcvBuffer->getRcvMsgNum() > 0)))
        {
            eventsToReport |= UDT_EPOLL_IN;
        }
        if (m_iSndBufSize > m_pSndBuffer->getCurrBufSize())
        {
            eventsToReport |= UDT_EPOLL_OUT;
        }
    }

    if (eventsToReport != 0)
        s_UDTUnited->m_EPoll.update_events(m_SocketId, m_sPollID, eventsToReport, true);
}

void CUDT::removeEPoll(const int eid)
{
    // clear IO events notifications;
    // since this happens after the epoll ID has been removed, they cannot be set again
    set<int> remove;
    remove.insert(eid);
    s_UDTUnited->m_EPoll.update_events(m_SocketId, remove, UDT_EPOLL_IN | UDT_EPOLL_OUT, false);

    {
        std::lock_guard<std::mutex> lk(s_UDTUnited->m_EPoll.m_EPollLock);
        m_sPollID.erase(eid);
    }

    if (auto synPacketHandler = m_synPacketHandler)
        synPacketHandler->removeEPoll(eid);
}

//-------------------------------------------------------------------------------------------------

Result<> CUDT::startup()
{
    s_UDTUnited = new CUDTUnited();
    return s_UDTUnited->initializeUdtLibrary();
}

Result<> CUDT::cleanup()
{
    auto result = s_UDTUnited->deinitializeUdtLibrary();
    delete s_UDTUnited;
    s_UDTUnited = nullptr;
    return result;
}

Result<UDTSOCKET> CUDT::socket(int af, int type, int)
{
    if (!s_UDTUnited->m_bGCStatus)
    {
        if (auto result = s_UDTUnited->initializeUdtLibrary(); !result.ok())
            return result.error();
    }

    return s_UDTUnited->newSocket(af, type);
}

Result<> CUDT::bind(UDTSOCKET u, const sockaddr* name, int namelen)
{
    return s_UDTUnited->bind(u, name, namelen);
}

Result<> CUDT::bind(UDTSOCKET u, UDPSOCKET udpsock)
{
    return s_UDTUnited->bind(u, udpsock);
}

Result<> CUDT::listen(UDTSOCKET u, int backlog)
{
    return s_UDTUnited->listen(u, backlog);
}

Result<UDTSOCKET> CUDT::accept(UDTSOCKET u, sockaddr* addr, int* addrlen)
{
    return s_UDTUnited->accept(u, addr, addrlen);
}

Result<> CUDT::connect(UDTSOCKET u, const sockaddr* name, int namelen)
{
    return s_UDTUnited->connect(u, name, namelen);
}

Result<> CUDT::shutdown(UDTSOCKET u, int how)
{
    return s_UDTUnited->shutdown(u, how);
}

Result<> CUDT::close(UDTSOCKET u)
{
    auto result = s_UDTUnited->close(u);
    if (!result.ok())
        s_UDTUnited->m_EPoll.RemoveEPollEvent(u);
    return result;
}

Result<> CUDT::getpeername(UDTSOCKET u, sockaddr* name, int* namelen)
{
    return s_UDTUnited->getpeername(u, name, namelen);
}

Result<> CUDT::getsockname(UDTSOCKET u, sockaddr* name, int* namelen)
{
    return s_UDTUnited->getsockname(u, name, namelen);;
}

Result<> CUDT::getsockopt(UDTSOCKET u, int, UDTOpt optname, void* optval, int* optlen)
{
    auto result = s_UDTUnited->lookup(u);
    if (!result.ok())
        return result.error();

    return result.get()->getOpt(optname, optval, *optlen);
}

Result<> CUDT::setsockopt(UDTSOCKET u, int, UDTOpt optname, const void* optval, int optlen)
{
    auto result = s_UDTUnited->lookup(u);
    if (!result.ok())
        return result.error();

    return result.get()->setOpt(optname, optval, optlen);
}

Result<int> CUDT::send(UDTSOCKET u, const char* buf, int len, int)
{
    auto result = s_UDTUnited->lookup(u);
    if (!result.ok())
        return result.error();

    return result.get()->send(buf, len);
}

Result<int> CUDT::recv(UDTSOCKET u, char* buf, int len, int)
{
    auto result = s_UDTUnited->lookup(u);
    if (!result.ok())
        return result.error();

    auto sendResult = result.get()->recv(buf, len);
    if (!sendResult.ok() || sendResult.get() == 0) //< Failure or connection closed.
        s_UDTUnited->m_EPoll.RemoveEPollEvent(u);
    // TODO: #akolesnikov What if non-critical error happens? E.g., WOULDBLOCK?

    return sendResult;
}

Result<int> CUDT::sendmsg(UDTSOCKET u, const char* buf, int len, int ttl, bool inorder)
{
    auto result = s_UDTUnited->lookup(u);
    if (!result.ok())
        return result.error();

    return result.get()->sendmsg(buf, len, std::chrono::milliseconds(ttl), inorder);
}

Result<int> CUDT::recvmsg(UDTSOCKET u, char* buf, int len)
{
    auto result = s_UDTUnited->lookup(u);
    if (!result.ok())
        return result.error();

    return result.get()->recvmsg(buf, len);
}

Result<int64_t> CUDT::sendfile(UDTSOCKET u, fstream& ifs, int64_t& offset, int64_t size, int block)
{
    auto result = s_UDTUnited->lookup(u);
    if (!result.ok())
        return result.error();

    return result.get()->sendfile(ifs, offset, size, block);
}

Result<int64_t> CUDT::recvfile(UDTSOCKET u, fstream& ofs, int64_t& offset, int64_t size, int block)
{
    auto result = s_UDTUnited->lookup(u);
    if (!result.ok())
        return result.error();

    return result.get()->recvfile(ofs, offset, size, block);
}

Result<int> CUDT::select(int, ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout)
{
    if ((nullptr == readfds) && (nullptr == writefds) && (nullptr == exceptfds))
        return Error(OsErrorCode::invalidData);

    return s_UDTUnited->select(readfds, writefds, exceptfds, timeout);
}

Result<int> CUDT::selectEx(
    const vector<UDTSOCKET>& fds,
    vector<UDTSOCKET>* readfds,
    vector<UDTSOCKET>* writefds,
    vector<UDTSOCKET>* exceptfds,
    int64_t msTimeOut)
{
    if ((nullptr == readfds) && (nullptr == writefds) && (nullptr == exceptfds))
        return Error(OsErrorCode::invalidData);

    return s_UDTUnited->selectEx(fds, readfds, writefds, exceptfds, msTimeOut);
}

Result<int> CUDT::epoll_create()
{
    return s_UDTUnited->epoll_create();
}

Result<void> CUDT::epoll_add_usock(const int eid, const UDTSOCKET u, const int* events)
{
    return s_UDTUnited->epoll_add_usock(eid, u, events);
}

Result<void> CUDT::epoll_add_ssock(const int eid, const SYSSOCKET s, const int* events)
{
    return s_UDTUnited->epoll_add_ssock(eid, s, events);
}

Result<void> CUDT::epoll_remove_usock(const int eid, const UDTSOCKET u)
{
    return s_UDTUnited->epoll_remove_usock(eid, u);
}

Result<void> CUDT::epoll_remove_ssock(const int eid, const SYSSOCKET s)
{
    return s_UDTUnited->epoll_remove_ssock(eid, s);
}

Result<int> CUDT::epoll_wait(
    const int eid,
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds,
    int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    return s_UDTUnited->epoll_wait(eid, readfds, writefds, msTimeOut, lrfds, lwfds);
}

Result<void> CUDT::epoll_interrupt_wait(const int eid)
{
    return s_UDTUnited->epoll_interrupt_wait(eid);
}

Result<> CUDT::epoll_release(const int eid)
{
    return s_UDTUnited->epoll_release(eid);
}

const Error& CUDT::getlasterror()
{
    return s_UDTUnited->getError();
}

Result<> CUDT::perfmon(UDTSOCKET u, CPerfMon* perf, bool clear)
{
    auto udt = s_UDTUnited->lookup(u);
    if (!udt.ok())
        return udt.error();

    return udt.get()->sample(perf, clear);
}

Result<std::shared_ptr<CUDT>> CUDT::getUDTHandle(UDTSOCKET u)
{
    return s_UDTUnited->lookup(u);
}

UDTSTATUS CUDT::getsockstate(UDTSOCKET u)
{
    return s_UDTUnited->getStatus(u);
}

void CUDT::initializeConnectedSocket(const detail::SocketAddress& addr)
{
    m_iPktSize = m_iMSS - 28;
    m_iPayloadSize = m_iPktSize - kPacketHeaderSize;

    m_pSndBuffer = std::make_unique<SendBuffer>(32, m_iPayloadSize);
    m_pRcvBuffer = std::make_unique<ReceiveBuffer>(m_iRcvBufSize);
    // after introducing lite ACK, the sndlosslist may not be cleared in time, so it requires twice space.
    m_pSndLossList = std::make_unique<CSndLossList>(m_iFlowWindowSize * 2);
    m_pRcvLossList = std::make_unique<CRcvLossList>(m_iFlightFlagSize);
    m_pACKWindow = std::make_unique<CACKWindow>(1024);
    m_pRcvTimeWindow = std::make_unique<CPktTimeWindow>(16, 64);
    m_pSndTimeWindow = std::make_unique<CPktTimeWindow>();

    CInfoBlock ib;
    ib.m_iIPversion = m_iIPversion;
    CInfoBlock::convert(addr, ib.m_piIP);
    if (m_pCache->lookup(ib.getKey(), &ib))
    {
        m_iRTT = ib.m_iRTT;
        m_iBandwidth = ib.m_iBandwidth;
    }

    m_congestionControl = CUDTCC();
    m_congestionControl.m_UDT = m_SocketId;
    m_congestionControl.setMSS(m_iMSS);
    m_congestionControl.setMaxCWndSize(m_iFlowWindowSize);
    m_congestionControl.setSndCurrSeqNo(m_iSndCurrSeqNo);
    m_congestionControl.setRcvRate(m_iDeliveryRate);
    m_congestionControl.setRTT(m_iRTT);
    m_congestionControl.setBandwidth(m_iBandwidth);
    m_congestionControl.init();

    m_ullInterval = std::chrono::microseconds((uint64_t)(m_congestionControl.m_dPktSndPeriod * m_ullCPUFrequency));
    m_dCongestionWindow = m_congestionControl.m_dCWndSize;

    m_bConnected = true;

    // register this socket for receiving data packets
    rcvQueue().addNewEntry(shared_from_this());
}

//-------------------------------------------------------------------------------------------------

ServerSideConnectionAcceptor::ServerSideConnectionAcceptor(
    std::chrono::microseconds startTime,
    UDTSockType sockType,
    UDTSOCKET socketId,
    CSndQueue* sndQueue,
    std::set<int> pollIds)
    :
    m_StartTime(startTime),
    m_iSockType(sockType),
    m_SocketId(socketId),
    m_sendQueue(sndQueue),
    m_pollIds(std::move(pollIds))
{
}

int ServerSideConnectionAcceptor::processConnectionRequest(
    const detail::SocketAddress& addr,
    const CPacket& packet)
{
    if (m_closing)
        return 1002;

    if (packet.getLength() != CHandShake::m_iContentSize)
        return 1004;

    CHandShake hs;
    hs.deserialize(packet.payload().data(), packet.getLength());

    // SYN cookie
    char clienthost[NI_MAXHOST];
    char clientport[NI_MAXSERV];
    getnameinfo(
        addr.get(), addr.size(),
        clienthost, sizeof(clienthost), clientport, sizeof(clientport),
        NI_NUMERICHOST | NI_NUMERICSERV);
    // secret changes every one minute.
    auto timestamp = std::chrono::duration_cast<std::chrono::minutes>(
        CTimer::getTime() - m_StartTime).count();
    stringstream cookiestr;
    cookiestr << clienthost << ":" << clientport << ":" << timestamp;
    unsigned char cookie[16];
    CMD5::compute(cookiestr.str().c_str(), cookie);

    if (1 == hs.m_iReqType)
    {
        auto response = packet;

        hs.m_iCookie = *(int*)cookie;
        response.m_iID = hs.m_iID;
        int size = response.getLength();
        hs.serialize(response.payload().data(), size);
        m_sendQueue->sendto(addr, std::move(response));
        return 0;
    }
    else
    {
        if (hs.m_iCookie != *(int*)cookie)
        {
            timestamp--;
            cookiestr << clienthost << ":" << clientport << ":" << timestamp;
            CMD5::compute(cookiestr.str().c_str(), cookie);

            if (hs.m_iCookie != *(int*)cookie)
                return -1;
        }
    }

    int32_t id = hs.m_iID;

    // When a peer side connects in...
    if ((PacketFlag::Control == packet.getFlag()) && (ControlPacketType::Handshake == packet.getType()))
    {
        if ((hs.m_iVersion != CUDT::m_iVersion) || (hs.m_iType != m_iSockType))
        {
            // mismatch, reject the request
            auto response = packet;
            hs.m_iReqType = 1002;
            int size = CHandShake::m_iContentSize;
            hs.serialize(response.payload().data(), size);
            response.m_iID = id;
            m_sendQueue->sendto(addr, std::move(response));
        }
        else
        {
            auto result = CUDT::s_UDTUnited->createConnection(m_SocketId, addr, &hs);
            if (!result.ok())
                hs.m_iReqType = 1002;

            // send back a response if connection failed or connection already existed
            // new connection response should be sent in connect()
            if (!result.ok() || result.get() != 1)
            {
                auto response = packet;
                int size = CHandShake::m_iContentSize;
                hs.serialize(response.payload().data(), size);
                response.m_iID = id;
                m_sendQueue->sendto(addr, std::move(response));
            }
            else
            {
                // a new connection has been created, enable epoll for write
                CUDT::s_UDTUnited->m_EPoll.update_events(m_SocketId, m_pollIds, UDT_EPOLL_OUT, true);
            }
        }
    }

    return hs.m_iReqType;
}

void ServerSideConnectionAcceptor::addEPoll(const int eid)
{
    std::lock_guard<std::mutex> lk(CUDT::s_UDTUnited->m_EPoll.m_EPollLock);
    m_pollIds.insert(eid);
}

void ServerSideConnectionAcceptor::removeEPoll(const int eid)
{
    std::lock_guard<std::mutex> lk(CUDT::s_UDTUnited->m_EPoll.m_EPollLock);
    m_pollIds.erase(eid);
}
