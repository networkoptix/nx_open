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
Yunhong Gu, last updated 07/09/2011
*****************************************************************************/

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef LEGACY_WIN32
#include <wspiapi.h>
#endif
#else
#include <unistd.h>
#endif

#include <cstring>
#include <functional>
#include <limits>

#include "api.h"
#include "core.h"
#include "multiplexer.h"

using namespace std;

CUDTSocket::CUDTSocket()
{
}

CUDTSocket::~CUDTSocket()
{
    m_pUDT = nullptr;
}

void CUDTSocket::addEPoll(const int eid)
{
    int additionalEvents = 0;
    if (m_Status == LISTENING && !m_pQueuedSockets.empty())
        additionalEvents = UDT_EPOLL_IN;
    m_pUDT->addEPoll(eid, additionalEvents);
}

void CUDTSocket::removeEPoll(const int eid)
{
    m_pUDT->removeEPoll(eid);
}

////////////////////////////////////////////////////////////////////////////////

static constexpr int kMinSocketId = 3;
static constexpr int kMaxSocketId = std::numeric_limits<int>::max() - 1;

CUDTUnited::CUDTUnited()
{
    // Socket ID MUST start from a random value
    srand((int) CTimer::getTime().count());
    m_SocketIdSequence = kMinSocketId + (int)((1 << 30) * (double(rand()) / RAND_MAX));

    m_cache = std::make_unique<CCache<CInfoBlock>>();
}

CUDTUnited::~CUDTUnited()
{
    m_Sockets.clear();
    m_ClosedSockets.clear();

    for (auto& idAndMultiplexer: m_multiplexers)
        idAndMultiplexer.second->shutdown();
    m_multiplexers.clear();
}

Result<> CUDTUnited::initializeUdtLibrary()
{
    std::lock_guard<std::mutex> lock(m_InitLock);

    if (m_iInstanceCount++ > 0)
        return success();

    // Global initialization code
#ifdef _WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);

    if (0 != WSAStartup(wVersionRequested, &wsaData))
        return Result<>(Error());
#endif

    //init CTimer::EventLock

    if (m_bGCStatus)
        return success();

    m_bClosing = false;

    m_GCThread = std::thread(std::bind(&CUDTUnited::garbageCollect, this));

    m_bGCStatus = true;

    return success();
}

Result<> CUDTUnited::deinitializeUdtLibrary()
{
    std::lock_guard<std::mutex> lock(m_InitLock);

    if (--m_iInstanceCount > 0)
        return success();

    //destroy CTimer::EventLock

    if (!m_bGCStatus)
        return success();

    {
        std::lock_guard<std::mutex> lock1(m_GCStopLock);
        m_bClosing = true;
        m_GCStopCond.notify_all();
    }
    m_GCThread.join();

    m_bGCStatus = false;

    // Global destruction code
#ifdef _WIN32
    WSACleanup();
#endif

    return success();
}

Result<UDTSOCKET> CUDTUnited::newSocket(int af, int type)
{
    if ((type != SOCK_STREAM) && (type != SOCK_DGRAM))
        return Error(OsErrorCode::protocolNotSupported);

    auto ns = std::make_shared<CUDTSocket>();
    ns->m_pUDT = std::make_shared<CUDT>();

    ns->m_SocketId = generateSocketId();

    ns->m_Status = INIT;
    ns->m_ListenSocket = 0;
    ns->m_pUDT->setSocket(
        ns->m_SocketId,
        (SOCK_STREAM == type) ? UDT_STREAM : UDT_DGRAM,
        ns->m_iIPversion = af);
    ns->m_pUDT->setCache(m_cache.get());

    // protect the m_Sockets structure.
    {
        std::unique_lock<std::mutex> lock(m_ControlLock);
        m_Sockets[ns->m_SocketId] = ns;
    }

    return success(ns->m_SocketId);
}

Result<int> CUDTUnited::createConnection(
    const UDTSOCKET listen,
    const detail::SocketAddress& remotePeerAddress,
    CHandShake* hs)
{
    std::shared_ptr<CUDTSocket> ls = locate(listen);
    if (!ls)
        return Error(OsErrorCode::badDescriptor);

    std::shared_ptr<CUDTSocket> ns;
    // if this connection has already been processed
    if (ns = locateSocketByHandshakeInfo(remotePeerAddress, hs->m_iID, hs->m_iISN); ns != nullptr)
    {
        if (ns->m_pUDT->broken())
        {
            // last connection from the "peer" address has been broken
            ns->m_Status = CLOSED;
            ns->m_TimeStamp = CTimer::getTime();

            std::unique_lock<std::mutex> acceptLocker(ls->m_AcceptLock);
            ls->m_pQueuedSockets.erase(ns->m_SocketId);
            ls->m_pAcceptSockets.erase(ns->m_SocketId);
        }
        else
        {
            // connection already exist, this is a repeated connection request
            // respond with existing HS information

            hs->m_iISN = ns->m_pUDT->isn();
            hs->m_iMSS = ns->m_pUDT->mss();
            hs->m_iFlightFlagSize = ns->m_pUDT->flightFlagSize();
            hs->m_iReqType = -1;
            hs->m_iID = ns->m_SocketId;

            return success(0);

            //except for this situation a new connection should be started
        }
    }

    // exceeding backlog, refuse the connection request
    if (ls->m_pQueuedSockets.size() >= ls->m_uiBackLog)
        return Error(OsErrorCode::connectionRefused);

    ns = std::make_shared<CUDTSocket>();
    ns->m_pUDT = std::make_shared<CUDT>(*(ls->m_pUDT));
    ns->m_pPeerAddr = remotePeerAddress;
    ns->m_pPeerAddr.get()->sa_family = ls->m_iIPversion;

    ns->m_SocketId = generateSocketId();

    ns->m_ListenSocket = listen;
    ns->m_iIPversion = ls->m_iIPversion;

    ns->m_pUDT->setSocket(
        ns->m_SocketId,
        ns->m_pUDT->sockType(),
        ns->m_pUDT->ipVersion());

    ns->m_PeerID = hs->m_iID;
    ns->m_iISN = hs->m_iISN;

    // bind to the same addr of listening socket
    ns->m_pUDT->open();
    auto result = updateMux(ns.get(), ls.get());
    if (result.ok())
        result = ns->m_pUDT->connect(remotePeerAddress, hs);
    if (!result.ok())
    {
        ns->m_pUDT->close();
        ns->m_Status = CLOSED;
        ns->m_TimeStamp = CTimer::getTime();

        return result.error();
    }

    ns->m_Status = CONNECTED;

    // copy address information of local node
    ns->m_pSelfAddr = ns->m_pUDT->sndQueue().getLocalAddr();
    CIPAddress::pton(&ns->m_pSelfAddr, ns->m_pUDT->selfIp(), ns->m_iIPversion);

    {
        std::unique_lock<std::mutex> lock(m_ControlLock);
        m_Sockets[ns->m_SocketId] = ns;
        saveSocketHandshakeInfo(lock, *ns);
    }

    {
        std::unique_lock<std::mutex> acceptLocker(ls->m_AcceptLock);
        ls->m_pQueuedSockets.insert(ns->m_SocketId);
    }

    // acknowledge users waiting for new connections on the listening socket
    m_EPoll.update_events(listen, ls->m_pUDT->pollIds(), UDT_EPOLL_IN, true);

    CTimer::triggerEvent();

    // wake up a waiting accept() call
    std::lock_guard<std::mutex> acceptLocker(ls->m_AcceptLock);
    ls->m_AcceptCond.notify_all();

    return success(1);
}

Result<std::shared_ptr<CUDT>> CUDTUnited::lookup(const UDTSOCKET u)
{
    // protects the m_Sockets structure
    std::lock_guard<std::mutex> lock(m_ControlLock);

    auto i = m_Sockets.find(u);
    if ((i == m_Sockets.end()) || (i->second->m_Status == CLOSED))
        return Error(OsErrorCode::badDescriptor);

    return success(i->second->m_pUDT);
}

UDTSTATUS CUDTUnited::getStatus(const UDTSOCKET u)
{
    // protects the m_Sockets structure
    std::lock_guard<std::mutex> lock(m_ControlLock);

    auto i = m_Sockets.find(u);
    if (i == m_Sockets.end())
    {
        if (m_ClosedSockets.find(u) != m_ClosedSockets.end())
            return CLOSED;

        return NONEXIST;
    }

    if (i->second->m_pUDT->broken())
        return BROKEN;

    return i->second->m_Status;
}

Result<> CUDTUnited::bind(const UDTSOCKET u, const sockaddr* name, int namelen)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (!s)
        return Error(OsErrorCode::badDescriptor);

    std::lock_guard<std::mutex> lock(s->m_ControlLock);

    // cannot bind a socket more than once
    if (INIT != s->m_Status)
        return Error(OsErrorCode::invalidData);

    // check the size of SOCKADDR structure
    if (AF_INET == s->m_iIPversion)
    {
        if (namelen != sizeof(sockaddr_in))
            return Error(OsErrorCode::invalidData);
    }
    else
    {
        if (namelen != sizeof(sockaddr_in6))
            return Error(OsErrorCode::invalidData);
    }

    s->m_pUDT->open();
    if (auto result = updateMux(s.get(), detail::SocketAddress(name, namelen)); !result.ok())
        return result;
    s->m_Status = OPENED;

    // copy address information of local node
    s->m_pSelfAddr = s->m_pUDT->sndQueue().getLocalAddr();

    return success();
}

Result<> CUDTUnited::bind(UDTSOCKET u, UDPSOCKET udpsock)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (!s)
        return Error(OsErrorCode::badDescriptor);

    std::lock_guard<std::mutex> lock(s->m_ControlLock);

    // cannot bind a socket more than once
    if (INIT != s->m_Status)
        return Error(OsErrorCode::invalidData);

    detail::SocketAddress socketAddress;

    if (-1 == ::getsockname(udpsock, socketAddress.get(), &socketAddress.length()))
        return OsError();

    s->m_pUDT->open();
    if (auto result = updateMux(s.get(), socketAddress, &udpsock); !result.ok())
        return result;
    s->m_Status = OPENED;

    // copy address information of local node
    s->m_pSelfAddr = s->m_pUDT->sndQueue().getLocalAddr();

    return success();
}

Result<> CUDTUnited::listen(const UDTSOCKET u, int backlog)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (!s)
        return Error(OsErrorCode::badDescriptor);

    std::lock_guard<std::mutex> lock(s->m_ControlLock);

    // do nothing if the socket is already listening
    if (LISTENING == s->m_Status)
        return success();

    // a socket can listen only if is in OPENED status
    if (OPENED != s->m_Status)
        return Error(OsErrorCode::invalidData);

    // listen is not supported in rendezvous connection setup
    if (s->m_pUDT->rendezvous())
        return Error(OsErrorCode::invalidData, UDT::ProtocolError::cannotListenInRendezvousMode);

    if (backlog <= 0)
        return Error(OsErrorCode::invalidData);

    s->m_uiBackLog = backlog;

    if (auto result = s->m_pUDT->listen(); !result.ok())
        return result;

    s->m_Status = LISTENING;

    return success();
}

Result<UDTSOCKET> CUDTUnited::accept(const UDTSOCKET listen, sockaddr* addr, int* addrlen)
{
    if ((nullptr != addr) && (nullptr == addrlen))
        return Error(OsErrorCode::invalidData);

    std::shared_ptr<CUDTSocket> ls = locate(listen);
    if (!ls)
        return Error(OsErrorCode::badDescriptor);

    // the "listen" socket must be in LISTENING status
    if (LISTENING != ls->m_Status)
        return Error(OsErrorCode::invalidData);

    // no "accept" in rendezvous connection setup
    if (ls->m_pUDT->rendezvous())
        return Error(OsErrorCode::invalidData, UDT::ProtocolError::cannotListenInRendezvousMode);

    UDTSOCKET u = CUDT::INVALID_SOCK;
    bool accepted = false;

    while (!accepted)
    {
        std::unique_lock<std::mutex> lk(ls->m_AcceptLock);

        if ((LISTENING != ls->m_Status) || ls->m_pUDT->broken())
        {
            // This socket has been closed.
            accepted = true;
        }
        else if (ls->m_pQueuedSockets.size() > 0)
        {
            u = *(ls->m_pQueuedSockets.begin());
            ls->m_pAcceptSockets.insert(ls->m_pAcceptSockets.end(), u);
            ls->m_pQueuedSockets.erase(ls->m_pQueuedSockets.begin());
            accepted = true;
        }
        else if (!ls->m_pUDT->synRecving())
        {
            accepted = true;
        }

        if (!accepted && (LISTENING == ls->m_Status))
            ls->m_AcceptCond.wait(lk);

        if (ls->m_pQueuedSockets.empty())
            m_EPoll.update_events(listen, ls->m_pUDT->pollIds(), UDT_EPOLL_IN, false);
    }

    if (u == CUDT::INVALID_SOCK)
    {
        // non-blocking receiving, no connection available
        if (!ls->m_pUDT->synRecving())
            return Error(OsErrorCode::wouldBlock);

        // listening socket is closed
        return Error(OsErrorCode::badDescriptor);
    }

    if ((addr != nullptr) && (addrlen != nullptr))
    {
        if (AF_INET == locate(u)->m_iIPversion)
            *addrlen = sizeof(sockaddr_in);
        else
            *addrlen = sizeof(sockaddr_in6);

        // copy address information of peer node
        memcpy(addr, &locate(u)->m_pPeerAddr, *addrlen);
    }

    return success(u);
}

Result<> CUDTUnited::connect(const UDTSOCKET u, const sockaddr* name, int namelen)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (!s)
        return Error(OsErrorCode::badDescriptor);

    std::lock_guard<std::mutex> lock(s->m_ControlLock);

    // check the size of SOCKADDR structure
    if (AF_INET == s->m_iIPversion)
    {
        if (namelen != sizeof(sockaddr_in))
            return Error(OsErrorCode::invalidData);
    }
    else
    {
        if (namelen != sizeof(sockaddr_in6))
            return Error(OsErrorCode::invalidData);
    }

    if (name->sa_family != s->m_iIPversion)
        return Error(OsErrorCode::invalidData);

    // a socket can "connect" only if it is in INIT or OPENED status
    if (INIT == s->m_Status)
    {
        if (!s->m_pUDT->rendezvous())
        {
            s->m_pUDT->open();
            if (auto result = updateMux(s.get()); !result.ok())
                return result;
            s->m_Status = OPENED;
        }
        else
        {
            return Error(OsErrorCode::invalidData);
        }
    }
    else if (OPENED != s->m_Status)
    {
        return Error(OsErrorCode::isConnected);
    }

    // connect_complete() may be called before connect() returns.
    // So we need to update the status before connect() is called,
    // otherwise the status may be overwritten with wrong value (CONNECTED vs. CONNECTING).
    s->m_Status = CONNECTING;
    auto result = s->m_pUDT->connect(detail::SocketAddress(name, namelen));
    if (!result.ok())
        return result;

    // record peer address
    s->m_pPeerAddr = detail::SocketAddress(name, namelen);
    s->m_pPeerAddr.setFamily(s->m_iIPversion);

    return success();
}

Result<> CUDTUnited::connect_complete(const UDTSOCKET u)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (!s)
        return Error(OsErrorCode::badDescriptor);

    // copy address information of local node
    // the local port must be correctly assigned BEFORE CUDT::connect(),
    // otherwise if connect() fails, the multiplexer cannot be located by garbage collection and will cause leak
    s->m_pSelfAddr = s->m_pUDT->sndQueue().getLocalAddr();
    // TODO: #akolesnikov Very strange. Same variable is assigned twice to different values.
    CIPAddress::pton(&s->m_pSelfAddr, s->m_pUDT->selfIp(), s->m_iIPversion);

    s->m_Status = CONNECTED;
    return success();
}

Result<> CUDTUnited::shutdown(const UDTSOCKET u, int how)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (!s)
        return Error(OsErrorCode::badDescriptor);

    return s->m_pUDT->shutdown(how);
}

Result<> CUDTUnited::close(const UDTSOCKET u)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (!s)
        return Error(OsErrorCode::badDescriptor);

    std::lock_guard<std::mutex> lock(s->m_ControlLock);

    if (s->m_Status == LISTENING)
    {
        if (s->m_pUDT->broken())
            return success();

        s->m_TimeStamp = CTimer::getTime();
        s->m_pUDT->setBroken(true);

        // broadcast all "accept" waiting
        {
            std::lock_guard<std::mutex> acceptLocker(s->m_AcceptLock);
            s->m_AcceptCond.notify_all();
        }

        s->m_pUDT->close();

        return success();
    }

    s->m_pUDT->close();

    // synchronize with garbage collection.
    std::lock_guard<std::mutex> manager_cg(m_ControlLock);

    // since "s" is located before m_ControlLock, locate it again in case it became invalid
    auto i = m_Sockets.find(u);
    if ((i == m_Sockets.end()) || (i->second->m_Status == CLOSED))
        return success();
    s = i->second;

    s->m_Status = CLOSED;

    // a socket will not be immediated removed when it is closed
    // in order to prevent other methods from accessing invalid address
    // a timer is started and the socket will be removed after approximately 1 second
    s->m_TimeStamp = CTimer::getTime();

    m_Sockets.erase(s->m_SocketId);
    m_ClosedSockets.emplace(s->m_SocketId, s);

    CTimer::triggerEvent();

    return success();
}

Result<> CUDTUnited::getpeername(const UDTSOCKET u, sockaddr* name, int* namelen)
{
    if (CONNECTED != getStatus(u))
        return Error(OsErrorCode::notConnected);

    std::shared_ptr<CUDTSocket> s = locate(u);
    if (!s)
        return Error(OsErrorCode::badDescriptor);

    if (!s->m_pUDT->connected() || s->m_pUDT->broken())
        return Error(OsErrorCode::notConnected);

    if (AF_INET == s->m_iIPversion)
        *namelen = sizeof(sockaddr_in);
    else
        *namelen = sizeof(sockaddr_in6);

    // copy address information of peer node
    memcpy(name, &s->m_pPeerAddr, *namelen);

    return success();
}

Result<> CUDTUnited::getsockname(const UDTSOCKET u, sockaddr* name, int* namelen)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (!s)
        return Error(OsErrorCode::badDescriptor);

    if (s->m_pUDT->broken())
        return Error(OsErrorCode::notConnected);

    if (INIT == s->m_Status)
        return Error(OsErrorCode::notConnected);

    // copy address information of local node
    s->m_pSelfAddr.copy(name, namelen);

    return success();
}

Result<int> CUDTUnited::select(ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout)
{
    const auto entertime = CTimer::getTime();

    std::chrono::microseconds to;
    if (nullptr == timeout)
        to = std::chrono::microseconds::max();
    else
        to = std::chrono::microseconds(timeout->tv_sec * 1000000 + timeout->tv_usec);

    // initialize results
    int count = 0;
    set<UDTSOCKET> rs, ws, es;

    // retrieve related UDT sockets
    vector<std::shared_ptr<CUDTSocket>> ru, wu, eu;
    std::shared_ptr<CUDTSocket> s;
    if (nullptr != readfds)
        for (auto i1 = readfds->begin(); i1 != readfds->end(); ++i1)
        {
            if (BROKEN == getStatus(*i1))
            {
                rs.insert(*i1);
                ++count;
            }
            else if (nullptr == (s = locate(*i1)))
            {
                return Error(OsErrorCode::badDescriptor);
            }
            else
            {
                ru.push_back(s);
            }
        }

    if (nullptr != writefds)
        for (auto i2 = writefds->begin(); i2 != writefds->end(); ++i2)
        {
            if (BROKEN == getStatus(*i2))
            {
                ws.insert(*i2);
                ++count;
            }
            else if (nullptr == (s = locate(*i2)))
            {
                return Error(OsErrorCode::badDescriptor);
            }
            else
            {
                wu.push_back(s);
            }
        }

    if (nullptr != exceptfds)
        for (auto i3 = exceptfds->begin(); i3 != exceptfds->end(); ++i3)
        {
            if (BROKEN == getStatus(*i3))
            {
                es.insert(*i3);
                ++count;
            }
            else if (nullptr == (s = locate(*i3)))
            {
                return Error(OsErrorCode::badDescriptor);
            }
            else
            {
                eu.push_back(s);
            }
        }

    do
    {
        // query read sockets
        for (auto j1 = ru.begin(); j1 != ru.end(); ++j1)
        {
            s = *j1;

            if ((s->m_pUDT->connected() && (s->m_pUDT->rcvBuffer()->getRcvDataSize() > 0) && ((s->m_pUDT->sockType() == UDT_STREAM) || (s->m_pUDT->rcvBuffer()->getRcvMsgNum() > 0)))
                || (!s->m_pUDT->listening() && (s->m_pUDT->broken() || !s->m_pUDT->connected()))
                || (s->m_pUDT->listening() && (s->m_pQueuedSockets.size() > 0))
                || (s->m_Status == CLOSED))
            {
                rs.insert(s->m_SocketId);
                ++count;
            }
        }

        // query write sockets
        for (auto j2 = wu.begin(); j2 != wu.end(); ++j2)
        {
            s = *j2;

            if ((s->m_pUDT->connected() && (s->m_pUDT->sndBuffer()->getCurrBufSize() < s->m_pUDT->sndBufSize()))
                || s->m_pUDT->broken() || !s->m_pUDT->connected() || (s->m_Status == CLOSED))
            {
                ws.insert(s->m_SocketId);
                ++count;
            }
        }

        // query exceptions on sockets
        for (auto j3 = eu.begin(); j3 != eu.end(); ++j3)
        {
            // check connection request status, not supported now
        }

        if (0 < count)
            break;

        CTimer::waitForEvent();
    } while (to > CTimer::getTime() - entertime);

    if (nullptr != readfds)
        *readfds = rs;

    if (nullptr != writefds)
        *writefds = ws;

    if (nullptr != exceptfds)
        *exceptfds = es;

    return success(count);
}

Result<int> CUDTUnited::selectEx(
    const vector<UDTSOCKET>& fds,
    vector<UDTSOCKET>* readfds,
    vector<UDTSOCKET>* writefds,
    vector<UDTSOCKET>* exceptfds,
    int64_t msTimeOut)
{
    const auto entertime = CTimer::getTime();

    std::chrono::microseconds to;
    if (msTimeOut >= 0)
        to = std::chrono::milliseconds(msTimeOut);
    else
        to = std::chrono::microseconds::max();

    // initialize results
    int count = 0;
    if (nullptr != readfds)
        readfds->clear();
    if (nullptr != writefds)
        writefds->clear();
    if (nullptr != exceptfds)
        exceptfds->clear();

    do
    {
        for (auto i = fds.begin(); i != fds.end(); ++i)
        {
            std::shared_ptr<CUDTSocket> s = locate(*i);

            if ((nullptr == s) || s->m_pUDT->broken() || (s->m_Status == CLOSED))
            {
                if (nullptr != exceptfds)
                {
                    exceptfds->push_back(*i);
                    ++count;
                }
                continue;
            }

            if (nullptr != readfds)
            {
                if ((s->m_pUDT->connected() && (s->m_pUDT->rcvBuffer()->getRcvDataSize() > 0) && ((s->m_pUDT->sockType() == UDT_STREAM) || (s->m_pUDT->rcvBuffer()->getRcvMsgNum() > 0)))
                    || (s->m_pUDT->listening() && (s->m_pQueuedSockets.size() > 0)))
                {
                    readfds->push_back(s->m_SocketId);
                    ++count;
                }
            }

            if (nullptr != writefds)
            {
                if (s->m_pUDT->connected() && (s->m_pUDT->sndBuffer()->getCurrBufSize() < s->m_pUDT->sndBufSize()))
                {
                    writefds->push_back(s->m_SocketId);
                    ++count;
                }
            }
        }

        if (count > 0)
            break;

        CTimer::waitForEvent();
    } while (to > CTimer::getTime() - entertime);

    return success(count);
}

Result<int> CUDTUnited::epoll_create()
{
    return m_EPoll.create();
}

Result<> CUDTUnited::epoll_release(const int eid)
{
    return m_EPoll.release(eid);
}

Result<> CUDTUnited::epoll_add_usock(const int eid, const UDTSOCKET u, const int* events)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (!s)
        return Error(OsErrorCode::badDescriptor);

    auto result = m_EPoll.add_usock(eid, u, events);
    s->addEPoll(eid);

    return result;
}

Result<> CUDTUnited::epoll_add_ssock(const int eid, const SYSSOCKET s, const int* events)
{
    return m_EPoll.add_ssock(eid, s, events);
}

Result<> CUDTUnited::epoll_remove_usock(const int eid, const UDTSOCKET u)
{
    auto result = m_EPoll.remove_usock(eid, u);

    std::shared_ptr<CUDTSocket> s = locate(u);
    if (s)
        s->removeEPoll(eid);

    return result;
}

Result<> CUDTUnited::epoll_remove_ssock(const int eid, const SYSSOCKET s)
{
    return m_EPoll.remove_ssock(eid, s);
}

Result<int> CUDTUnited::epoll_wait(
    const int eid,
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds, int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    return m_EPoll.wait(eid, readfds, writefds, msTimeOut, lrfds, lwfds);
}

Result<> CUDTUnited::epoll_interrupt_wait(const int eid)
{
    return m_EPoll.interruptWait(eid);
}

std::shared_ptr<CUDTSocket> CUDTUnited::locate(const UDTSOCKET u)
{
    std::lock_guard<std::mutex> lock(m_ControlLock);

    auto i = m_Sockets.find(u);
    if ((i == m_Sockets.end()) || (i->second->m_Status == CLOSED))
        return nullptr;

    return i->second;
}

void CUDTUnited::saveSocketHandshakeInfo(
    const std::unique_lock<std::mutex>& /*lock*/,
    const CUDTSocket& sock)
{
    m_PeerRec[HandshakeKey{sock.m_PeerID, sock.m_iISN}].insert(sock.m_SocketId);
}

std::shared_ptr<CUDTSocket> CUDTUnited::locateSocketByHandshakeInfo(
    const detail::SocketAddress& peer,
    const UDTSOCKET id,
    int32_t isn)
{
    std::lock_guard<std::mutex> lock(m_ControlLock);

    auto it = m_PeerRec.find(HandshakeKey{id, isn});
    if (it == m_PeerRec.end())
        return nullptr;

    for (const auto& sockId: it->second)
    {
        auto sockIt = m_Sockets.find(sockId);
        // this socket might have been closed and moved m_ClosedSockets
        if (sockIt == m_Sockets.end())
            continue;

        if (peer == sockIt->second->m_pPeerAddr)
            return sockIt->second;
    }

    return nullptr;
}

void CUDTUnited::removeSocketHandshakeInfo(
    const std::unique_lock<std::mutex>& /*lock*/,
    const CUDTSocket& sock)
{
    auto it = m_PeerRec.find(HandshakeKey{sock.m_PeerID, sock.m_iISN});
    if (it == m_PeerRec.end())
        return;

    it->second.erase(sock.m_SocketId);
    if (it->second.empty())
        m_PeerRec.erase(it);
}

static constexpr std::chrono::microseconds kSomeUnclearTimeout = std::chrono::seconds(3);

void CUDTUnited::checkBrokenSockets()
{
    std::unique_lock<std::mutex> cg(m_ControlLock);

    // set of sockets To Be Closed and To Be Removed
    vector<UDTSOCKET> tbc;
    vector<UDTSOCKET> tbr;

    for (auto i = m_Sockets.begin(); i != m_Sockets.end(); ++i)
    {
        // check broken connection
        if (i->second->m_pUDT->broken())
        {
            if (i->second->m_Status == LISTENING)
            {
                // for a listening socket, it should wait an extra 3 seconds in case a client is connecting
                if (CTimer::getTime() - i->second->m_TimeStamp < kSomeUnclearTimeout)
                    continue;
            }
            else if ((i->second->m_pUDT->rcvBuffer() != nullptr)
                && (i->second->m_pUDT->rcvBuffer()->getRcvDataSize() > 0)
                && (i->second->m_pUDT->decrementBrokenCounter() > 0))
            {
                // if there is still data in the receiver buffer, wait longer
                continue;
            }

            //close broken connections and start removal timer
            i->second->m_Status = CLOSED;
            i->second->m_TimeStamp = CTimer::getTime();
            tbc.push_back(i->first);
            m_ClosedSockets[i->first] = i->second;

            // signal epoll , however user cannot perform any pending operations
            // after this wired loop, the socket for this connection is just broken.
            // I cannot fix this problem since if I delay the deletion, the thread
            // will crash !
            if (i->second->m_pUDT->shutdown())
                m_EPoll.update_events(i->first, i->second->m_pUDT->pollIds(), UDT_EPOLL_IN | UDT_EPOLL_ERR, true);

            // remove from listener's queue
            auto ls = m_Sockets.find(i->second->m_ListenSocket);
            if (ls == m_Sockets.end())
            {
                ls = m_ClosedSockets.find(i->second->m_ListenSocket);
                if (ls == m_ClosedSockets.end())
                    continue;
            }

            std::unique_lock<std::mutex> acceptLocker(ls->second->m_AcceptLock);
            ls->second->m_pQueuedSockets.erase(i->second->m_SocketId);
            ls->second->m_pAcceptSockets.erase(i->second->m_SocketId);
        }
    }

    for (auto j = m_ClosedSockets.begin(); j != m_ClosedSockets.end(); ++j)
    {
        if (j->second->m_pUDT->lingerExpiration() > std::chrono::microseconds::zero())
        {
            // asynchronous close:
            if ((nullptr == j->second->m_pUDT->sndBuffer())
                || (0 == j->second->m_pUDT->sndBuffer()->getCurrBufSize())
                || (j->second->m_pUDT->lingerExpiration() <= CTimer::getTime()))
            {
                j->second->m_pUDT->setLingerExpiration(std::chrono::microseconds::zero());
                j->second->m_pUDT->setIsClosing(true);
                j->second->m_TimeStamp = CTimer::getTime();
            }
        }

        static constexpr auto kDestroyTimeout = std::chrono::seconds(1);

        // timeout 1 second to destroy a socket AND it has been removed from RcvUList
        if (CTimer::getTime() - j->second->m_TimeStamp > kDestroyTimeout)
            tbr.push_back(j->first);
    }

    // move closed sockets to the ClosedSockets structure
    for (auto k = tbc.begin(); k != tbc.end(); ++k)
        m_Sockets.erase(*k);

    // remove those timed out sockets
    for (auto l = tbr.begin(); l != tbr.end(); ++l)
        removeSocket(cg, *l);

    std::vector<std::shared_ptr<Multiplexer>> multiplexersToRemove = selectMultiplexersToRemove();

    cg.unlock();

    // Removing multiplexer with no mutex locked since it implies waiting for send/receive thread to exit
    for (auto& multiplexer: multiplexersToRemove)
        multiplexer->shutdown();
}

void CUDTUnited::removeSocket(
    const std::unique_lock<std::mutex>& lock,
    const UDTSOCKET u)
{
    auto i = m_ClosedSockets.find(u);

    // invalid socket ID
    if (i == m_ClosedSockets.end())
        return;

    // decrease multiplexer reference count, and remove it if necessary
    const int mid = i->second->m_multiplexerId;

    {
        std::unique_lock<std::mutex> lk(i->second->m_AcceptLock);

        // if it is a listener, close all un-accepted sockets in its queue and remove them later
        for (auto q = i->second->m_pQueuedSockets.begin(); q != i->second->m_pQueuedSockets.end(); ++q)
        {
            m_Sockets[*q]->m_pUDT->setBroken(true);
            m_Sockets[*q]->m_pUDT->close();
            m_Sockets[*q]->m_TimeStamp = CTimer::getTime();
            m_Sockets[*q]->m_Status = CLOSED;
            m_ClosedSockets[*q] = m_Sockets[*q];
            m_Sockets.erase(*q);
        }
    }

    // remove from peer rec
    removeSocketHandshakeInfo(lock, *i->second);

    // delete this one
    i->second->m_pUDT->close();
    m_ClosedSockets.erase(i);

    auto m = m_multiplexers.find(mid);
    if (m == m_multiplexers.end())
    {
        //something is wrong!!!
        return;
    }
}

std::vector<std::shared_ptr<Multiplexer>> CUDTUnited::selectMultiplexersToRemove()
{
    std::vector<std::shared_ptr<Multiplexer>> result;

    for (auto it = m_multiplexers.begin(); it != m_multiplexers.end();)
    {
        auto& multiplexer = it->second;

        // Note: this code relies on the fact that std::weak_ptr<Multiplexer> is not used anywhere.
        // This weakness will be addressed in a future version.
        if (multiplexer.use_count() == 1)
        {
            result.push_back(multiplexer);
            it = m_multiplexers.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return result;
}

void CUDTUnited::setError(Error e)
{
    std::lock_guard<std::mutex> lk(m_TLSLock);
    m_mTLSRecord[GetCurrentThreadId()] = e;
}

const Error& CUDTUnited::getError() const
{
    std::lock_guard<std::mutex> lk(m_TLSLock);

    const auto it = m_mTLSRecord.find(GetCurrentThreadId());
    if (it != m_mTLSRecord.end())
        return it->second;

    static Error noError(OsErrorCode::ok, UDT::ProtocolError::none);
    return noError;
}

#ifdef _WIN32
void CUDTUnited::cleanupPerThreadLastErrors()
{
    std::lock_guard<std::mutex> lk(m_TLSLock);

    vector<DWORD> tbr;
    for (auto i = m_mTLSRecord.begin(); i != m_mTLSRecord.end(); ++i)
    {
        HANDLE h = OpenThread(THREAD_QUERY_INFORMATION, FALSE, i->first);
        if (nullptr == h)
        {
            tbr.push_back(i->first);
            break;
        }

        if (WAIT_OBJECT_0 == WaitForSingleObject(h, 0))
            tbr.push_back(i->first);

        CloseHandle(h);
    }

    for (auto j = tbr.begin(); j != tbr.end(); ++j)
        m_mTLSRecord.erase(*j);
}
#endif

Result<> CUDTUnited::updateMux(
    CUDTSocket* s,
    const std::optional<detail::SocketAddress>& addr,
    const UDPSOCKET* udpsock)
{
    std::lock_guard<std::mutex> lock(m_ControlLock);

    if ((s->m_pUDT->reuseAddr()) && addr)
    {
        const auto port = addr->port();

        // find a reusable address
        for (auto i = m_multiplexers.begin(); i != m_multiplexers.end(); ++i)
        {
            auto& multiplexer = i->second;

            if ((multiplexer->ipVersion == s->m_pUDT->ipVersion()) &&
                (multiplexer->maximumSegmentSize == s->m_pUDT->mss()) && multiplexer->reusable)
            {
                if (multiplexer->udpPort() == port)
                {
                    // reuse the existing multiplexer
                    s->m_pUDT->setMultiplexer(multiplexer);
                    s->m_multiplexerId = multiplexer->id;
                    return success();
                }
            }
        }
    }

    // a new multiplexer is needed
    auto multiplexer = std::make_shared<Multiplexer>(
        s->m_pUDT->ipVersion(),
        s->m_pUDT->payloadSize(),
        s->m_pUDT->mss(),
        s->m_pUDT->reuseAddr(),
        s->m_SocketId);

    multiplexer->channel().setSndBufSize(s->m_pUDT->udpSndBufSize());
    multiplexer->channel().setRcvBufSize(s->m_pUDT->udpRcvBufSize());

    auto result = udpsock
        ? multiplexer->channel().open(*udpsock)
        : multiplexer->channel().open(addr);
    if (!result.ok())
    {
        multiplexer->channel().shutdown();
        return result;
    }

    m_multiplexers[multiplexer->id] = multiplexer;

    s->m_pUDT->setMultiplexer(multiplexer);
    s->m_multiplexerId = multiplexer->id;

    if (auto result = multiplexer->start(); !result.ok())
    {
        multiplexer->channel().shutdown();
        return result;
    }

    return success();
}

UDTSOCKET CUDTUnited::generateSocketId()
{
    std::unique_lock<std::mutex> lock(m_IDLock);

    // Rotating socket sequence.
    if (m_SocketIdSequence <= kMinSocketId)
        m_SocketIdSequence = kMaxSocketId;

    return --m_SocketIdSequence;
}

Result<> CUDTUnited::updateMux(CUDTSocket* s, const CUDTSocket* ls)
{
    std::lock_guard<std::mutex> lock(m_ControlLock);

    const int port = ls->m_pSelfAddr.port();

    // find the listener's address
    for (auto i = m_multiplexers.begin(); i != m_multiplexers.end(); ++i)
    {
        auto& multiplexer = i->second;
        if (multiplexer->udpPort() == port)
        {
            // reuse the existing multiplexer
            s->m_pUDT->setMultiplexer(multiplexer);
            s->m_multiplexerId = multiplexer->id;
            return success();
        }
    }

    return success();
}

static constexpr std::chrono::milliseconds kGarbageCollectTickPeriod(100);

void CUDTUnited::garbageCollect()
{
    setCurrentThreadName(typeid(*this).name());

    std::unique_lock<std::mutex> gcguard(m_GCStopLock);

    while (!m_bClosing)
    {
        checkBrokenSockets();

#ifdef _WIN32
        cleanupPerThreadLastErrors();
#endif

        m_GCStopCond.wait_for(gcguard, kGarbageCollectTickPeriod);
    }

    // remove all sockets and multiplexers
    {
        std::lock_guard<std::mutex> controlLocker(m_ControlLock);
        for (auto i = m_Sockets.begin(); i != m_Sockets.end(); ++i)
        {
            i->second->m_pUDT->setBroken(true);
            i->second->m_pUDT->close();
            i->second->m_Status = CLOSED;
            i->second->m_TimeStamp = CTimer::getTime();
            m_ClosedSockets[i->first] = i->second;

            // remove from listener's queue
            auto ls = m_Sockets.find(i->second->m_ListenSocket);
            if (ls == m_Sockets.end())
            {
                ls = m_ClosedSockets.find(i->second->m_ListenSocket);
                if (ls == m_ClosedSockets.end())
                    continue;
            }

            std::unique_lock<std::mutex> lk(ls->second->m_AcceptLock);
            ls->second->m_pQueuedSockets.erase(i->second->m_SocketId);
            ls->second->m_pAcceptSockets.erase(i->second->m_SocketId);
        }
        m_Sockets.clear();

        for (auto j = m_ClosedSockets.begin(); j != m_ClosedSockets.end(); ++j)
            j->second->m_TimeStamp = std::chrono::microseconds::zero();
    }

    for (;;)
    {
        checkBrokenSockets();

        std::unique_lock<std::mutex> lock(m_ControlLock);
        bool empty = m_ClosedSockets.empty();
        lock.unlock();

        if (empty)
            break;

        CTimer::sleep();
    }
}
