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
#include "api.h"
#include "core.h"
#include "multiplexer.h"

using namespace std;

CUDTSocket::CUDTSocket()
{
    memset(&m_pPeerAddr, 0, sizeof(m_pPeerAddr));
}

CUDTSocket::~CUDTSocket()
{
    if (AF_INET == m_iIPversion)
    {
        delete (sockaddr_in*)m_pSelfAddr;
    }
    else
    {
        delete (sockaddr_in6*)m_pSelfAddr;
    }

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

CUDTUnited::CUDTUnited():
    m_TLSError(),
    m_MultiplexerLock()
{
    // Socket ID MUST start from a random value
    srand((unsigned int)CTimer::getTime());
    m_SocketId = 1 + (int)((1 << 30) * (double(rand()) / RAND_MAX));

#ifndef _WIN32
    pthread_key_create(&m_TLSError, TLSDestroy);
#else
    m_TLSError = TlsAlloc();
    m_TLSLock = CreateMutex(nullptr, false, nullptr);
#endif

    m_cache = std::make_unique<CCache<CInfoBlock>>();
}

CUDTUnited::~CUDTUnited()
{
#ifndef _WIN32
    pthread_key_delete(m_TLSError);
#else
    TlsFree(m_TLSError);
    CloseHandle(m_TLSLock);
#endif

    m_cache.reset();
}

int CUDTUnited::startup()
{
    std::lock_guard<std::mutex> lock(m_InitLock);

    if (m_iInstanceCount++ > 0)
        return 0;

    // Global initialization code
#ifdef _WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);

    if (0 != WSAStartup(wVersionRequested, &wsaData))
        throw CUDTException(1, 0, WSAGetLastError());
#endif

    //init CTimer::EventLock

    if (m_bGCStatus)
        return true;

    m_bClosing = false;

    m_GCThread = std::thread(std::bind(&CUDTUnited::garbageCollect, this));

    m_bGCStatus = true;

    return 0;
}

int CUDTUnited::cleanup()
{
    std::lock_guard<std::mutex> lock(m_InitLock);

    if (--m_iInstanceCount > 0)
        return 0;

    //destroy CTimer::EventLock

    if (!m_bGCStatus)
        return 0;

    {
        std::lock_guard<std::mutex> lock(m_GCStopLock);
        m_bClosing = true;
        m_GCStopCond.notify_all();
    }
    m_GCThread.join();

    m_bGCStatus = false;

    // Global destruction code
#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}

UDTSOCKET CUDTUnited::newSocket(int af, int type)
{
    if ((type != SOCK_STREAM) && (type != SOCK_DGRAM))
        throw CUDTException(5, 3, 0);

    std::shared_ptr<CUDTSocket> ns;

    try
    {
        ns = std::make_shared<CUDTSocket>();
        ns->m_pUDT = std::make_shared<CUDT>();
        if (AF_INET == af)
        {
            ns->m_pSelfAddr = (sockaddr*)(new sockaddr_in);
            ((sockaddr_in*)(ns->m_pSelfAddr))->sin_port = 0;
        }
        else
        {
            ns->m_pSelfAddr = (sockaddr*)(new sockaddr_in6);
            ((sockaddr_in6*)(ns->m_pSelfAddr))->sin6_port = 0;
        }
    }
    catch (...)
    {
        throw CUDTException(3, 2, 0);
    }

    {
        std::unique_lock<std::mutex> lock(m_IDLock);
        ns->m_SocketId = --m_SocketId;
    }

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
        try
        {
            m_Sockets[ns->m_SocketId] = ns;
        }
        catch (...)
        {
            //failure and rollback
            lock.unlock();
        }
    }

    if (nullptr == ns)
        throw CUDTException(3, 2, 0);

    return ns->m_SocketId;
}

int CUDTUnited::newConnection(const UDTSOCKET listen, const sockaddr* peer, CHandShake* hs)
{
    std::shared_ptr<CUDTSocket> ns;
    std::shared_ptr<CUDTSocket> ls = locate(listen);

    if (nullptr == ls)
        return -1;

    // if this connection has already been processed
    if (nullptr != (ns = locate(peer, hs->m_iID, hs->m_iISN)))
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

            return 0;

            //except for this situation a new connection should be started
        }
    }

    // exceeding backlog, refuse the connection request
    if (ls->m_pQueuedSockets.size() >= ls->m_uiBackLog)
        return -1;

    try
    {
        ns = std::make_shared<CUDTSocket>();
        ns->m_pUDT = std::make_shared<CUDT>(*(ls->m_pUDT));
        if (AF_INET == ls->m_iIPversion)
        {
            ns->m_pSelfAddr = (sockaddr*)(new sockaddr_in);
            ((sockaddr_in*)(ns->m_pSelfAddr))->sin_port = 0;
            memcpy(&ns->m_pPeerAddr, peer, sizeof(*peer));
        }
        else
        {
            ns->m_pSelfAddr = (sockaddr*)(new sockaddr_in6);
            ((sockaddr_in6*)(ns->m_pSelfAddr))->sin6_port = 0;
            memcpy(&ns->m_pPeerAddr, peer, sizeof(*peer));
        }
        ns->m_pPeerAddr.sa_family = ls->m_iIPversion;
    }
    catch (...)
    {
        return -1;
    }

    {
        std::unique_lock<std::mutex> lock(m_IDLock);
        ns->m_SocketId = --m_SocketId;
    }

    ns->m_ListenSocket = listen;
    ns->m_iIPversion = ls->m_iIPversion;

    ns->m_pUDT->setSocket(
        ns->m_SocketId,
        ns->m_pUDT->sockType(),
        ns->m_pUDT->ipVersion());

    ns->m_PeerID = hs->m_iID;
    ns->m_iISN = hs->m_iISN;

    int error = 0;

    try
    {
        // bind to the same addr of listening socket
        ns->m_pUDT->open();
        updateMux(ns.get(), ls.get());
        ns->m_pUDT->connect(peer, hs);
    }
    catch (...)
    {
        error = 1;
        goto ERR_ROLLBACK;
    }

    ns->m_Status = CONNECTED;

    // copy address information of local node
    ns->m_pUDT->sndQueue().channel()->getSockAddr(ns->m_pSelfAddr);
    CIPAddress::pton(ns->m_pSelfAddr, ns->m_pUDT->selfIp(), ns->m_iIPversion);

    {
        // protect the m_Sockets structure.
        std::lock_guard<std::mutex> lock(m_ControlLock);
        try
        {
            m_Sockets[ns->m_SocketId] = ns;
            m_PeerRec[(ns->m_PeerID << 30) + ns->m_iISN].insert(ns->m_SocketId);
        }
        catch (...)
        {
            error = 2;
        }
    }

    {
        std::unique_lock<std::mutex> acceptLocker(ls->m_AcceptLock);
        try
        {
            ls->m_pQueuedSockets.insert(ns->m_SocketId);
        }
        catch (...)
        {
            error = 3;
        }
    }

    // acknowledge users waiting for new connections on the listening socket
    m_EPoll.update_events(listen, ls->m_pUDT->pollIds(), UDT_EPOLL_IN, true);

    CTimer::triggerEvent();

ERR_ROLLBACK:
    if (error > 0)
    {
        ns->m_pUDT->close();
        ns->m_Status = CLOSED;
        ns->m_TimeStamp = CTimer::getTime();

        return -1;
    }

    // wake up a waiting accept() call
    std::lock_guard<std::mutex> acceptLocker(ls->m_AcceptLock);
    ls->m_AcceptCond.notify_all();

    return 1;
}

std::shared_ptr<CUDT> CUDTUnited::lookup(const UDTSOCKET u)
{
    // protects the m_Sockets structure
    std::lock_guard<std::mutex> lock(m_ControlLock);

    auto i = m_Sockets.find(u);

    if ((i == m_Sockets.end()) || (i->second->m_Status == CLOSED))
        throw CUDTException(5, 4, 0);

    return i->second->m_pUDT;
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

int CUDTUnited::bind(const UDTSOCKET u, const sockaddr* name, int namelen)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (nullptr == s)
        throw CUDTException(5, 4, 0);

    std::lock_guard<std::mutex> lock(s->m_ControlLock);

    // cannot bind a socket more than once
    if (INIT != s->m_Status)
        throw CUDTException(5, 0, 0);

    // check the size of SOCKADDR structure
    if (AF_INET == s->m_iIPversion)
    {
        if (namelen != sizeof(sockaddr_in))
            throw CUDTException(5, 3, 0);
    }
    else
    {
        if (namelen != sizeof(sockaddr_in6))
            throw CUDTException(5, 3, 0);
    }

    s->m_pUDT->open();
    updateMux(s.get(), name);
    s->m_Status = OPENED;

    // copy address information of local node
    s->m_pUDT->sndQueue().channel()->getSockAddr(s->m_pSelfAddr);

    return 0;
}

int CUDTUnited::bind(UDTSOCKET u, UDPSOCKET udpsock)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (nullptr == s)
        throw CUDTException(5, 4, 0);

    std::lock_guard<std::mutex> lock(s->m_ControlLock);

    // cannot bind a socket more than once
    if (INIT != s->m_Status)
        throw CUDTException(5, 0, 0);

    sockaddr_in name4;
    sockaddr_in6 name6;
    sockaddr* name = nullptr;
    socklen_t namelen = 0;

    if (AF_INET == s->m_iIPversion)
    {
        namelen = sizeof(sockaddr_in);
        name = (sockaddr*)&name4;
    }
    else
    {
        namelen = sizeof(sockaddr_in6);
        name = (sockaddr*)&name6;
    }

    if (-1 == ::getsockname(udpsock, name, &namelen))
        throw CUDTException(5, 3);

    s->m_pUDT->open();
    updateMux(s.get(), name, &udpsock);
    s->m_Status = OPENED;

    // copy address information of local node
    s->m_pUDT->sndQueue().channel()->getSockAddr(s->m_pSelfAddr);

    return 0;
}

int CUDTUnited::listen(const UDTSOCKET u, int backlog)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (nullptr == s)
        throw CUDTException(5, 4, 0);

    std::lock_guard<std::mutex> lock(s->m_ControlLock);

    // do nothing if the socket is already listening
    if (LISTENING == s->m_Status)
        return 0;

    // a socket can listen only if is in OPENED status
    if (OPENED != s->m_Status)
        throw CUDTException(5, 5, 0);

    // listen is not supported in rendezvous connection setup
    if (s->m_pUDT->rendezvous())
        throw CUDTException(5, 7, 0);

    if (backlog <= 0)
        throw CUDTException(5, 3, 0);

    s->m_uiBackLog = backlog;

    s->m_pUDT->listen();

    s->m_Status = LISTENING;

    return 0;
}

UDTSOCKET CUDTUnited::accept(const UDTSOCKET listen, sockaddr* addr, int* addrlen)
{
    if ((nullptr != addr) && (nullptr == addrlen))
        throw CUDTException(5, 3, 0);

    std::shared_ptr<CUDTSocket> ls = locate(listen);

    if (ls == nullptr)
        throw CUDTException(5, 4, 0);

    // the "listen" socket must be in LISTENING status
    if (LISTENING != ls->m_Status)
        throw CUDTException(5, 6, 0);

    // no "accept" in rendezvous connection setup
    if (ls->m_pUDT->rendezvous())
        throw CUDTException(5, 7, 0);

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
            throw CUDTException(6, 2, 0);

        // listening socket is closed
        throw CUDTException(5, 6, 0);
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

    return u;
}

int CUDTUnited::connect(const UDTSOCKET u, const sockaddr* name, int namelen)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (nullptr == s)
        throw CUDTException(5, 4, 0);

    std::lock_guard<std::mutex> lock(s->m_ControlLock);

    // check the size of SOCKADDR structure
    if (AF_INET == s->m_iIPversion)
    {
        if (namelen != sizeof(sockaddr_in))
            throw CUDTException(5, 3, 0);
    }
    else
    {
        if (namelen != sizeof(sockaddr_in6))
            throw CUDTException(5, 3, 0);
    }

    // a socket can "connect" only if it is in INIT or OPENED status
    if (INIT == s->m_Status)
    {
        if (!s->m_pUDT->rendezvous())
        {
            s->m_pUDT->open();
            updateMux(s.get());
            s->m_Status = OPENED;
        }
        else
            throw CUDTException(5, 8, 0);
    }
    else if (OPENED != s->m_Status)
        throw CUDTException(5, 2, 0);

    // connect_complete() may be called before connect() returns.
    // So we need to update the status before connect() is called,
    // otherwise the status may be overwritten with wrong value (CONNECTED vs. CONNECTING).
    s->m_Status = CONNECTING;
    try
    {
        s->m_pUDT->connect(name);
    }
    catch (CUDTException e)
    {
        s->m_Status = OPENED;
        throw e;
    }

    // record peer address
    memcpy(&s->m_pPeerAddr, name, sizeof(*name));
    s->m_pPeerAddr.sa_family = s->m_iIPversion;

    return 0;
}

void CUDTUnited::connect_complete(const UDTSOCKET u)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (nullptr == s)
        throw CUDTException(5, 4, 0);

    // copy address information of local node
    // the local port must be correctly assigned BEFORE CUDT::connect(),
    // otherwise if connect() fails, the multiplexer cannot be located by garbage collection and will cause leak
    s->m_pUDT->sndQueue().channel()->getSockAddr(s->m_pSelfAddr);
    CIPAddress::pton(s->m_pSelfAddr, s->m_pUDT->selfIp(), s->m_iIPversion);

    s->m_Status = CONNECTED;
}

int CUDTUnited::shutdown(const UDTSOCKET u, int how)
{
    std::shared_ptr<CUDTSocket> s = locate(u);

    if (nullptr == s)
        throw CUDTException(5, 4, 0);

    return s->m_pUDT->shutdown(how);
}

int CUDTUnited::close(const UDTSOCKET u)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    if (nullptr == s)
        throw CUDTException(5, 4, 0);

    std::lock_guard<std::mutex> lock(s->m_ControlLock);

    if (s->m_Status == LISTENING)
    {
        if (s->m_pUDT->broken())
            return 0;

        s->m_TimeStamp = CTimer::getTime();
        s->m_pUDT->setBroken(true);

        // broadcast all "accept" waiting
        {
            std::lock_guard<std::mutex> acceptLocker(s->m_AcceptLock);
            s->m_AcceptCond.notify_all();
        }

        s->m_pUDT->close();

        return 0;
    }

    s->m_pUDT->close();

    // synchronize with garbage collection.
    std::lock_guard<std::mutex> manager_cg(m_ControlLock);

    // since "s" is located before m_ControlLock, locate it again in case it became invalid
    auto i = m_Sockets.find(u);
    if ((i == m_Sockets.end()) || (i->second->m_Status == CLOSED))
        return 0;
    s = i->second;

    s->m_Status = CLOSED;

    // a socket will not be immediated removed when it is closed
    // in order to prevent other methods from accessing invalid address
    // a timer is started and the socket will be removed after approximately 1 second
    s->m_TimeStamp = CTimer::getTime();

    m_Sockets.erase(s->m_SocketId);
    m_ClosedSockets.emplace(s->m_SocketId, s);

    CTimer::triggerEvent();

    return 0;
}

int CUDTUnited::getpeername(const UDTSOCKET u, sockaddr* name, int* namelen)
{
    if (CONNECTED != getStatus(u))
        throw CUDTException(2, 2, 0);

    std::shared_ptr<CUDTSocket> s = locate(u);

    if (nullptr == s)
        throw CUDTException(5, 4, 0);

    if (!s->m_pUDT->connected() || s->m_pUDT->broken())
        throw CUDTException(2, 2, 0);

    if (AF_INET == s->m_iIPversion)
        *namelen = sizeof(sockaddr_in);
    else
        *namelen = sizeof(sockaddr_in6);

    // copy address information of peer node
    memcpy(name, &s->m_pPeerAddr, *namelen);

    return 0;
}

int CUDTUnited::getsockname(const UDTSOCKET u, sockaddr* name, int* namelen)
{
    std::shared_ptr<CUDTSocket> s = locate(u);

    if (nullptr == s)
        throw CUDTException(5, 4, 0);

    if (s->m_pUDT->broken())
        throw CUDTException(5, 4, 0);

    if (INIT == s->m_Status)
        throw CUDTException(2, 2, 0);

    if (AF_INET == s->m_iIPversion)
        *namelen = sizeof(sockaddr_in);
    else
        *namelen = sizeof(sockaddr_in6);

    // copy address information of local node
    memcpy(name, s->m_pSelfAddr, *namelen);

    return 0;
}

int CUDTUnited::select(ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout)
{
    uint64_t entertime = CTimer::getTime();

    uint64_t to;
    if (nullptr == timeout)
        to = 0xFFFFFFFFFFFFFFFFULL;
    else
        to = timeout->tv_sec * 1000000 + timeout->tv_usec;

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
                throw CUDTException(5, 4, 0);
            else
                ru.push_back(s);
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
                throw CUDTException(5, 4, 0);
            else
                wu.push_back(s);
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
                throw CUDTException(5, 4, 0);
            else
                eu.push_back(s);
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

    return count;
}

int CUDTUnited::selectEx(
    const vector<UDTSOCKET>& fds,
    vector<UDTSOCKET>* readfds,
    vector<UDTSOCKET>* writefds,
    vector<UDTSOCKET>* exceptfds,
    int64_t msTimeOut)
{
    uint64_t entertime = CTimer::getTime();

    uint64_t to;
    if (msTimeOut >= 0)
        to = msTimeOut * 1000;
    else
        to = 0xFFFFFFFFFFFFFFFFULL;

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

    return count;
}

int CUDTUnited::epoll_create()
{
    return m_EPoll.create();
}

int CUDTUnited::epoll_add_usock(const int eid, const UDTSOCKET u, const int* events)
{
    std::shared_ptr<CUDTSocket> s = locate(u);
    int ret = -1;
    if (nullptr != s)
    {
        ret = m_EPoll.add_usock(eid, u, events);
        s->addEPoll(eid);
    }
    else
    {
        throw CUDTException(5, 4);
    }

    return ret;
}

int CUDTUnited::epoll_add_ssock(const int eid, const SYSSOCKET s, const int* events)
{
    return m_EPoll.add_ssock(eid, s, events);
}

int CUDTUnited::epoll_remove_usock(const int eid, const UDTSOCKET u)
{
    int ret = m_EPoll.remove_usock(eid, u);

    std::shared_ptr<CUDTSocket> s = locate(u);
    if (nullptr != s)
    {
        s->removeEPoll(eid);
    }

    return ret;
}

int CUDTUnited::epoll_remove_ssock(const int eid, const SYSSOCKET s)
{
    return m_EPoll.remove_ssock(eid, s);
}

int CUDTUnited::epoll_wait(
    const int eid,
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds, int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    return m_EPoll.wait(eid, readfds, writefds, msTimeOut, lrfds, lwfds);
}

int CUDTUnited::epoll_interrupt_wait(const int eid)
{
    return m_EPoll.interruptWait(eid);
}

int CUDTUnited::epoll_release(const int eid)
{
    return m_EPoll.release(eid);
}

std::shared_ptr<CUDTSocket> CUDTUnited::locate(const UDTSOCKET u)
{
    std::lock_guard<std::mutex> lock(m_ControlLock);

    auto i = m_Sockets.find(u);

    if ((i == m_Sockets.end()) || (i->second->m_Status == CLOSED))
        return nullptr;

    return i->second;
}

std::shared_ptr<CUDTSocket> CUDTUnited::locate(
    const sockaddr* peer,
    const UDTSOCKET id,
    int32_t isn)
{
    std::lock_guard<std::mutex> lock(m_ControlLock);

    auto i = m_PeerRec.find((id << 30) + isn);
    if (i == m_PeerRec.end())
        return nullptr;

    for (auto j = i->second.begin(); j != i->second.end(); ++j)
    {
        auto k = m_Sockets.find(*j);
        // this socket might have been closed and moved m_ClosedSockets
        if (k == m_Sockets.end())
            continue;

        if (CIPAddress::ipcmp(peer, &k->second->m_pPeerAddr))
            return k->second;
    }

    return nullptr;
}

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
                if (CTimer::getTime() - i->second->m_TimeStamp < 3000000)
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
        if (j->second->m_pUDT->lingerExpiration() > 0)
        {
            // asynchronous close:
            if ((nullptr == j->second->m_pUDT->sndBuffer())
                || (0 == j->second->m_pUDT->sndBuffer()->getCurrBufSize())
                || (j->second->m_pUDT->lingerExpiration() <= CTimer::getTime()))
            {
                j->second->m_pUDT->setLingerExpiration(0);
                j->second->m_pUDT->setIsClosing(true);
                j->second->m_TimeStamp = CTimer::getTime();
            }
        }

        // timeout 1 second to destroy a socket AND it has been removed from RcvUList
        if (CTimer::getTime() - j->second->m_TimeStamp > 1000000)
            tbr.push_back(j->first);
    }

    // move closed sockets to the ClosedSockets structure
    for (auto k = tbc.begin(); k != tbc.end(); ++k)
        m_Sockets.erase(*k);

    std::vector<std::shared_ptr<Multiplexer>> multiplexersToRemove;
    // remove those timed out sockets
    for (auto l = tbr.begin(); l != tbr.end(); ++l)
        removeSocket(*l, &multiplexersToRemove);

    cg.unlock();

    // Removing multiplexer with no mutex locked since it implies waiting for send/receive thread to exit
    for (auto& multiplexer: multiplexersToRemove)
        multiplexer->shutdown();
}

void CUDTUnited::removeSocket(
    const UDTSOCKET u,
    std::vector<std::shared_ptr<Multiplexer>>* const multiplexersToRemove)
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
    auto j = m_PeerRec.find((i->second->m_PeerID << 30) + i->second->m_iISN);
    if (j != m_PeerRec.end())
    {
        j->second.erase(u);
        if (j->second.empty())
            m_PeerRec.erase(j);
    }

    // delete this one
    i->second->m_pUDT->close();
    m_ClosedSockets.erase(i);

    auto m = m_multiplexers.find(mid);
    if (m == m_multiplexers.end())
    {
        //something is wrong!!!
        return;
    }

    m->second->refCount--;
    if (0 == m->second->refCount)
    {
        multiplexersToRemove->push_back(m->second);
        m_multiplexers.erase(m);
    }
}

void CUDTUnited::setError(CUDTException* e)
{
#ifndef _WIN32
    delete (CUDTException*)pthread_getspecific(m_TLSError);
    pthread_setspecific(m_TLSError, e);
#else
    CGuard tg(m_TLSLock);
    delete (CUDTException*)TlsGetValue(m_TLSError);
    TlsSetValue(m_TLSError, e);
    m_mTLSRecord[GetCurrentThreadId()] = e;
#endif
}

CUDTException* CUDTUnited::getError()
{
#ifndef _WIN32
    if (nullptr == pthread_getspecific(m_TLSError))
        pthread_setspecific(m_TLSError, new CUDTException);
    return (CUDTException*)pthread_getspecific(m_TLSError);
#else
    CGuard tg(m_TLSLock);
    if (nullptr == TlsGetValue(m_TLSError))
    {
        CUDTException* e = new CUDTException;
        TlsSetValue(m_TLSError, e);
        m_mTLSRecord[GetCurrentThreadId()] = e;
    }
    return (CUDTException*)TlsGetValue(m_TLSError);
#endif
}

#ifdef _WIN32
void CUDTUnited::checkTLSValue()
{
    CGuard tg(m_TLSLock);

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
        {
            delete i->second;
            tbr.push_back(i->first);
        }
        CloseHandle(h);
    }
    for (auto j = tbr.begin(); j != tbr.end(); ++j)
        m_mTLSRecord.erase(*j);
}
#endif

void CUDTUnited::updateMux(CUDTSocket* s, const sockaddr* addr, const UDPSOCKET* udpsock)
{
    std::lock_guard<std::mutex> lock(m_ControlLock);

    if ((s->m_pUDT->reuseAddr()) && (nullptr != addr))
    {
        int port = (AF_INET == s->m_pUDT->ipVersion())
            ? ntohs(((sockaddr_in*)addr)->sin_port)
            : ntohs(((sockaddr_in6*)addr)->sin6_port);

        // find a reusable address
        for (auto i = m_multiplexers.begin(); i != m_multiplexers.end(); ++i)
        {
            auto& multiplexer = i->second;

            if ((multiplexer->ipVersion == s->m_pUDT->ipVersion()) &&
                (multiplexer->maximumSegmentSize == s->m_pUDT->mss()) && multiplexer->reusable)
            {
                if (multiplexer->udpPort == port)
                {
                    // reuse the existing multiplexer
                    ++multiplexer->refCount;
                    s->m_pUDT->setMultiplexer(multiplexer);
                    s->m_multiplexerId = multiplexer->id;
                    return;
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

    ++multiplexer->refCount;

    multiplexer->channel().setSndBufSize(s->m_pUDT->udpSndBufSize());
    multiplexer->channel().setRcvBufSize(s->m_pUDT->udpRcvBufSize());

    try
    {
        if (nullptr != udpsock)
            multiplexer->channel().open(*udpsock);
        else
            multiplexer->channel().open(addr);
    }
    catch (CUDTException& e)
    {
        multiplexer->channel().shutdown();
        throw e;
    }

    struct sockaddr sa;
    memset(&sa, 0, sizeof(sa));
    multiplexer->channel().getSockAddr(&sa);
    multiplexer->udpPort = (AF_INET == sa.sa_family)
        ? ntohs(((sockaddr_in*)&sa)->sin_port)
        : ntohs(((sockaddr_in6*)&sa)->sin6_port);

    m_multiplexers[multiplexer->id] = multiplexer;

    s->m_pUDT->setMultiplexer(multiplexer);
    s->m_multiplexerId = multiplexer->id;

    multiplexer->start();
}

void CUDTUnited::updateMux(CUDTSocket* s, const CUDTSocket* ls)
{
    std::lock_guard<std::mutex> lock(m_ControlLock);

    int port = (AF_INET == ls->m_iIPversion)
        ? ntohs(((sockaddr_in*)ls->m_pSelfAddr)->sin_port)
        : ntohs(((sockaddr_in6*)ls->m_pSelfAddr)->sin6_port);

    // find the listener's address
    for (auto i = m_multiplexers.begin(); i != m_multiplexers.end(); ++i)
    {
        auto& multiplexer = i->second;
        if (multiplexer->udpPort == port)
        {
            // reuse the existing multiplexer
            ++multiplexer->refCount;
            s->m_pUDT->setMultiplexer(multiplexer);
            s->m_multiplexerId = multiplexer->id;
            return;
        }
    }
}

static constexpr std::chrono::milliseconds kGarbageCollectTickPeriod(100);

void CUDTUnited::garbageCollect()
{
    std::unique_lock<std::mutex> gcguard(m_GCStopLock);

    while (!m_bClosing)
    {
        checkBrokenSockets();

#ifdef _WIN32
        checkTLSValue();
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
            j->second->m_TimeStamp = 0;
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

//-------------------------------------------------------------------------------------------------

int CUDT::startup()
{
    s_UDTUnited = new CUDTUnited();
    return s_UDTUnited->startup();
}

int CUDT::cleanup()
{
    const auto result = s_UDTUnited->cleanup();
    delete s_UDTUnited;
    s_UDTUnited = nullptr;
    return result;
}

UDTSOCKET CUDT::socket(int af, int type, int)
{
    if (!s_UDTUnited->m_bGCStatus)
        s_UDTUnited->startup();

    try
    {
        return s_UDTUnited->newSocket(af, type);
    }
    catch (CUDTException& e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return INVALID_SOCK;
    }
    catch (bad_alloc&)
    {
        s_UDTUnited->setError(new CUDTException(3, 2, 0));
        return INVALID_SOCK;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return INVALID_SOCK;
    }
}

int CUDT::bind(UDTSOCKET u, const sockaddr* name, int namelen)
{
    try
    {
        return s_UDTUnited->bind(u, name, namelen);
    }
    catch (CUDTException& e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (bad_alloc&)
    {
        s_UDTUnited->setError(new CUDTException(3, 2, 0));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::bind(UDTSOCKET u, UDPSOCKET udpsock)
{
    try
    {
        return s_UDTUnited->bind(u, udpsock);
    }
    catch (CUDTException& e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (bad_alloc&)
    {
        s_UDTUnited->setError(new CUDTException(3, 2, 0));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::listen(UDTSOCKET u, int backlog)
{
    try
    {
        return s_UDTUnited->listen(u, backlog);
    }
    catch (CUDTException& e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (bad_alloc&)
    {
        s_UDTUnited->setError(new CUDTException(3, 2, 0));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

UDTSOCKET CUDT::accept(UDTSOCKET u, sockaddr* addr, int* addrlen)
{
    try
    {
        return s_UDTUnited->accept(u, addr, addrlen);
    }
    catch (CUDTException& e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return INVALID_SOCK;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return INVALID_SOCK;
    }
}

int CUDT::connect(UDTSOCKET u, const sockaddr* name, int namelen)
{
    try
    {
        return s_UDTUnited->connect(u, name, namelen);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (bad_alloc&)
    {
        s_UDTUnited->setError(new CUDTException(3, 2, 0));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::shutdown(UDTSOCKET u, int how)
{
    try
    {
        return s_UDTUnited->shutdown(u, how);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::close(UDTSOCKET u)
{
    try
    {
        return s_UDTUnited->close(u);
    }
    catch (CUDTException e)
    {
        // For a broken connection, there's no way to close it normally, UDT
        // will remove all the UDTSOCKET file descriptor internally there and
        // we have no way to notify UDT that this epoll filed is done as well.
        s_UDTUnited->m_EPoll.RemoveEPollEvent(u);
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::getpeername(UDTSOCKET u, sockaddr* name, int* namelen)
{
    try
    {
        return s_UDTUnited->getpeername(u, name, namelen);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::getsockname(UDTSOCKET u, sockaddr* name, int* namelen)
{
    try
    {
        return s_UDTUnited->getsockname(u, name, namelen);;
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::getsockopt(UDTSOCKET u, int, UDTOpt optname, void* optval, int* optlen)
{
    try
    {
        auto udt = s_UDTUnited->lookup(u);
        udt->getOpt(optname, optval, *optlen);
        return 0;
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::setsockopt(UDTSOCKET u, int, UDTOpt optname, const void* optval, int optlen)
{
    try
    {
        auto udt = s_UDTUnited->lookup(u);
        udt->setOpt(optname, optval, optlen);
        return 0;
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::send(UDTSOCKET u, const char* buf, int len, int)
{
    try
    {
        auto udt = s_UDTUnited->lookup(u);
        return udt->send(buf, len);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (bad_alloc&)
    {
        s_UDTUnited->setError(new CUDTException(3, 2, 0));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::recv(UDTSOCKET u, char* buf, int len, int)
{
    try
    {
        auto udt = s_UDTUnited->lookup(u);
        int ret = udt->recv(buf, len);
        if (ret <= 0)
        {
            s_UDTUnited->m_EPoll.RemoveEPollEvent(u);
        }
        return ret;
    }
    catch (CUDTException e)
    {
        // This deletion is needed since library doesn't
        // take care of the EPOLL notification properly
        // And I have no way to distinguish a CRASH and
        // a clean close right now. Currently only a ugly
        // hack in our code works.
        s_UDTUnited->m_EPoll.RemoveEPollEvent(u);
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::sendmsg(UDTSOCKET u, const char* buf, int len, int ttl, bool inorder)
{
    try
    {
        auto udt = s_UDTUnited->lookup(u);
        return udt->sendmsg(buf, len, ttl, inorder);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (bad_alloc&)
    {
        s_UDTUnited->setError(new CUDTException(3, 2, 0));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::recvmsg(UDTSOCKET u, char* buf, int len)
{
    try
    {
        auto udt = s_UDTUnited->lookup(u);
        return udt->recvmsg(buf, len);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int64_t CUDT::sendfile(UDTSOCKET u, fstream& ifs, int64_t& offset, int64_t size, int block)
{
    try
    {
        auto udt = s_UDTUnited->lookup(u);
        return udt->sendfile(ifs, offset, size, block);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (bad_alloc&)
    {
        s_UDTUnited->setError(new CUDTException(3, 2, 0));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int64_t CUDT::recvfile(UDTSOCKET u, fstream& ofs, int64_t& offset, int64_t size, int block)
{
    try
    {
        auto udt = s_UDTUnited->lookup(u);
        return udt->recvfile(ofs, offset, size, block);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::select(int, ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout)
{
    if ((nullptr == readfds) && (nullptr == writefds) && (nullptr == exceptfds))
    {
        s_UDTUnited->setError(new CUDTException(5, 3, 0));
        return ERROR;
    }

    try
    {
        return s_UDTUnited->select(readfds, writefds, exceptfds, timeout);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (bad_alloc&)
    {
        s_UDTUnited->setError(new CUDTException(3, 2, 0));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::selectEx(const vector<UDTSOCKET>& fds, vector<UDTSOCKET>* readfds, vector<UDTSOCKET>* writefds, vector<UDTSOCKET>* exceptfds, int64_t msTimeOut)
{
    if ((nullptr == readfds) && (nullptr == writefds) && (nullptr == exceptfds))
    {
        s_UDTUnited->setError(new CUDTException(5, 3, 0));
        return ERROR;
    }

    try
    {
        return s_UDTUnited->selectEx(fds, readfds, writefds, exceptfds, msTimeOut);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (bad_alloc&)
    {
        s_UDTUnited->setError(new CUDTException(3, 2, 0));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::epoll_create()
{
    try
    {
        return s_UDTUnited->epoll_create();
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::epoll_add_usock(const int eid, const UDTSOCKET u, const int* events)
{
    try
    {
        return s_UDTUnited->epoll_add_usock(eid, u, events);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::epoll_add_ssock(const int eid, const SYSSOCKET s, const int* events)
{
    try
    {
        return s_UDTUnited->epoll_add_ssock(eid, s, events);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::epoll_remove_usock(const int eid, const UDTSOCKET u)
{
    try
    {
        return s_UDTUnited->epoll_remove_usock(eid, u);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::epoll_remove_ssock(const int eid, const SYSSOCKET s)
{
    try
    {
        return s_UDTUnited->epoll_remove_ssock(eid, s);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::epoll_wait(
    const int eid,
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds, int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    try
    {
        return s_UDTUnited->epoll_wait(eid, readfds, writefds, msTimeOut, lrfds, lwfds);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::epoll_interrupt_wait(const int eid)
{
    try
    {
        return s_UDTUnited->epoll_interrupt_wait(eid);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

int CUDT::epoll_release(const int eid)
{
    try
    {
        return s_UDTUnited->epoll_release(eid);
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

CUDTException& CUDT::getlasterror()
{
    return *s_UDTUnited->getError();
}

int CUDT::perfmon(UDTSOCKET u, CPerfMon* perf, bool clear)
{
    try
    {
        auto udt = s_UDTUnited->lookup(u);
        udt->sample(perf, clear);
        return 0;
    }
    catch (CUDTException e)
    {
        s_UDTUnited->setError(new CUDTException(e));
        return ERROR;
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return ERROR;
    }
}

std::shared_ptr<CUDT> CUDT::getUDTHandle(UDTSOCKET u)
{
    try
    {
        return s_UDTUnited->lookup(u);
    }
    catch (...)
    {
        return nullptr;
    }
}

UDTSTATUS CUDT::getsockstate(UDTSOCKET u)
{
    try
    {
        return s_UDTUnited->getStatus(u);
    }
    catch (...)
    {
        s_UDTUnited->setError(new CUDTException(-1, 0, 0));
        return NONEXIST;
    }
}


////////////////////////////////////////////////////////////////////////////////

namespace UDT {

int startup()
{
    return CUDT::startup();
}

int cleanup()
{
    return CUDT::cleanup();
}

UDTSOCKET socket(int af, int type, int protocol)
{
    return CUDT::socket(af, type, protocol);
}

int bind(UDTSOCKET u, const struct sockaddr* name, int namelen)
{
    return CUDT::bind(u, name, namelen);
}

int bind2(UDTSOCKET u, UDPSOCKET udpsock)
{
    return CUDT::bind(u, udpsock);
}

int listen(UDTSOCKET u, int backlog)
{
    return CUDT::listen(u, backlog);
}

UDTSOCKET accept(UDTSOCKET u, struct sockaddr* addr, int* addrlen)
{
    return CUDT::accept(u, addr, addrlen);
}

int connect(UDTSOCKET u, const struct sockaddr* name, int namelen)
{
    return CUDT::connect(u, name, namelen);
}

int shutdown(UDTSOCKET u, int how)
{
    return CUDT::shutdown(u, how);
}

int close(UDTSOCKET u)
{
    return CUDT::close(u);
}

int getpeername(UDTSOCKET u, struct sockaddr* name, int* namelen)
{
    return CUDT::getpeername(u, name, namelen);
}

int getsockname(UDTSOCKET u, struct sockaddr* name, int* namelen)
{
    return CUDT::getsockname(u, name, namelen);
}

int getsockopt(UDTSOCKET u, int level, SOCKOPT optname, void* optval, int* optlen)
{
    return CUDT::getsockopt(u, level, optname, optval, optlen);
}

int setsockopt(UDTSOCKET u, int level, SOCKOPT optname, const void* optval, int optlen)
{
    return CUDT::setsockopt(u, level, optname, optval, optlen);
}

int send(UDTSOCKET u, const char* buf, int len, int flags)
{
    return CUDT::send(u, buf, len, flags);
}

int recv(UDTSOCKET u, char* buf, int len, int flags)
{
    return CUDT::recv(u, buf, len, flags);
}

int sendmsg(UDTSOCKET u, const char* buf, int len, int ttl, bool inorder)
{
    return CUDT::sendmsg(u, buf, len, ttl, inorder);
}

int recvmsg(UDTSOCKET u, char* buf, int len)
{
    return CUDT::recvmsg(u, buf, len);
}

int64_t sendfile(UDTSOCKET u, fstream& ifs, int64_t& offset, int64_t size, int block)
{
    return CUDT::sendfile(u, ifs, offset, size, block);
}

int64_t recvfile(UDTSOCKET u, fstream& ofs, int64_t& offset, int64_t size, int block)
{
    return CUDT::recvfile(u, ofs, offset, size, block);
}

int64_t sendfile2(UDTSOCKET u, const char* path, int64_t* offset, int64_t size, int block)
{
    fstream ifs(path, ios::binary | ios::in);
    int64_t ret = CUDT::sendfile(u, ifs, *offset, size, block);
    ifs.close();
    return ret;
}

int64_t recvfile2(UDTSOCKET u, const char* path, int64_t* offset, int64_t size, int block)
{
    fstream ofs(path, ios::binary | ios::out);
    int64_t ret = CUDT::recvfile(u, ofs, *offset, size, block);
    ofs.close();
    return ret;
}

int select(int nfds, UDSET* readfds, UDSET* writefds, UDSET* exceptfds, const struct timeval* timeout)
{
    return CUDT::select(nfds, readfds, writefds, exceptfds, timeout);
}

int selectEx(const vector<UDTSOCKET>& fds, vector<UDTSOCKET>* readfds, vector<UDTSOCKET>* writefds, vector<UDTSOCKET>* exceptfds, int64_t msTimeOut)
{
    return CUDT::selectEx(fds, readfds, writefds, exceptfds, msTimeOut);
}

int epoll_create()
{
    return CUDT::epoll_create();
}

int epoll_add_usock(int eid, UDTSOCKET u, const int* events)
{
    return CUDT::epoll_add_usock(eid, u, events);
}

int epoll_add_ssock(int eid, SYSSOCKET s, const int* events)
{
    return CUDT::epoll_add_ssock(eid, s, events);
}

int epoll_remove_usock(int eid, UDTSOCKET u)
{
    return CUDT::epoll_remove_usock(eid, u);
}

int epoll_remove_ssock(int eid, SYSSOCKET s)
{
    return CUDT::epoll_remove_ssock(eid, s);
}

int epoll_wait(
    int eid,
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds, int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    return CUDT::epoll_wait(eid, readfds, writefds, msTimeOut, lrfds, lwfds);
}

int epoll_interrupt_wait(int eid)
{
    return CUDT::epoll_interrupt_wait(eid);
}

#define SET_RESULT(val, num, fds, it) \
   if ((val != nullptr) && !val->empty()) \
   { \
      if (*num > static_cast<int>(val->size())) \
         *num = val->size(); \
      int count = 0; \
      for (it = val->begin(); it != val->end(); ++ it) \
      { \
         if (count >= *num) \
            break; \
         fds[count ++] = it->first; \
      } \
   }
int epoll_wait2(int eid, UDTSOCKET* readfds, int* rnum, UDTSOCKET* writefds, int* wnum, int64_t msTimeOut,
    SYSSOCKET* lrfds, int* lrnum, SYSSOCKET* lwfds, int* lwnum)
{
    // This API is an alternative format for epoll_wait, created for compatability with other languages.
    // Users need to pass in an array for holding the returned sockets, with the maximum array length
    // stored in *rnum, etc., which will be updated with returned number of sockets.

    map<UDTSOCKET, int> readset;
    map<UDTSOCKET, int> writeset;
    map<SYSSOCKET, int> lrset;
    map<SYSSOCKET, int> lwset;
    map<UDTSOCKET, int>* rval = nullptr;
    map<UDTSOCKET, int>* wval = nullptr;
    map<SYSSOCKET, int>* lrval = nullptr;
    map<SYSSOCKET, int>* lwval = nullptr;
    if ((readfds != nullptr) && (rnum != nullptr))
        rval = &readset;
    if ((writefds != nullptr) && (wnum != nullptr))
        wval = &writeset;
    if ((lrfds != nullptr) && (lrnum != nullptr))
        lrval = &lrset;
    if ((lwfds != nullptr) && (lwnum != nullptr))
        lwval = &lwset;

    int ret = CUDT::epoll_wait(eid, rval, wval, msTimeOut, lrval, lwval);
    if (ret > 0)
    {
        map<UDTSOCKET, int>::const_iterator i;
        SET_RESULT(rval, rnum, readfds, i);
        SET_RESULT(wval, wnum, writefds, i);
        map<SYSSOCKET, int>::const_iterator j;
        SET_RESULT(lrval, lrnum, lrfds, j);
        SET_RESULT(lwval, lwnum, lwfds, j);
    }
    return ret;
}

int epoll_release(int eid)
{
    return CUDT::epoll_release(eid);
}

ERRORINFO& getlasterror()
{
    return CUDT::getlasterror();
}

int getlasterror_code()
{
    return CUDT::getlasterror().getErrorCode();
}

const char* getlasterror_desc()
{
    return CUDT::getlasterror().getErrorMessage();
}

int perfmon(UDTSOCKET u, TRACEINFO* perf, bool clear)
{
    return CUDT::perfmon(u, perf, clear);
}

UDTSTATUS getsockstate(UDTSOCKET u)
{
    return CUDT::getsockstate(u);
}

}  // namespace UDT
