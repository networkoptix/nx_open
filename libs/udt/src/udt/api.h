/*****************************************************************************
Copyright (c) 2001 - 2010, The Board of Trustees of the University of Illinois.
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
Yunhong Gu, last updated 09/28/2010
*****************************************************************************/

#ifndef __UDT_API_H__
#define __UDT_API_H__


#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "udt.h"
#include "packet.h"
#include "queue.h"
#include "cache.h"
#include "epoll.h"
#include "socket_addresss.h"

class CUDT;
class Multiplexer;

class CUDTSocket
{
public:
    CUDTSocket();
    ~CUDTSocket();

    void addEPoll(const int eid);
    void removeEPoll(const int eid);

    UDTSTATUS m_Status = INIT;                       // current socket state

    uint64_t m_TimeStamp = 0;                     // time when the socket is closed

    int m_iIPversion = 0;                         // IP version
    detail::SocketAddress m_pSelfAddr;                    // pointer to the local address of the socket
    detail::SocketAddress m_pPeerAddr;                    // pointer to the peer address of the socket

    UDTSOCKET m_SocketId = 0;                     // socket ID
    UDTSOCKET m_ListenSocket = 0;                 // ID of the listener socket; 0 means this is an independent socket

    UDTSOCKET m_PeerID = 0;                       // peer socket ID
    int32_t m_iISN = 0;                           // initial sequence number, used to tell different connection from same IP:port

    std::shared_ptr<CUDT> m_pUDT;                             // pointer to the UDT entity

    std::set<UDTSOCKET> m_pQueuedSockets;    // set of connections waiting for accept()
    std::set<UDTSOCKET> m_pAcceptSockets;    // set of accept()ed connections

    std::condition_variable m_AcceptCond;     // used to block "accept" call
    std::mutex m_AcceptLock;                  // mutex associated to m_AcceptCond

    unsigned int m_uiBackLog = 0;                 // maximum number of connections in queue

    int m_multiplexerId = -1;                             // multiplexer ID

    std::mutex m_ControlLock;            // lock this socket exclusively for control APIs: bind/listen/connect

private:
    CUDTSocket(const CUDTSocket&);
    CUDTSocket& operator=(const CUDTSocket&);
};

////////////////////////////////////////////////////////////////////////////////

class CUDTUnited
{
    friend class CUDT;
    friend class CRendezvousQueue;
    friend class ServerSideConnectionAcceptor;

public:
    CUDTUnited();
    ~CUDTUnited();

public:

    Result<> initializeUdtLibrary();

    Result<> deinitializeUdtLibrary();

    // Functionality:
    //    Create a new UDT socket.
    // Parameters:
    //    0) [in] af: IP version, IPv4 (AF_INET) or IPv6 (AF_INET6).
    //    1) [in] type: socket type, SOCK_STREAM or SOCK_DGRAM
    // Returned value:
    //    The new UDT socket ID, or INVALID_SOCK.

    Result<UDTSOCKET> newSocket(int af, int type);

    // Parameters:
    //    0) [in] listen: the listening UDT socket;
    //    1) [in] peer: peer address.
    //    2) [in/out] hs: handshake information from peer side (in), negotiated value (out);
    // Returned value:
    //    If the new connection is successfully created: 1 success, 0 already exist.

    Result<int> createConnection(
        const UDTSOCKET listen,
        const detail::SocketAddress& remotePeerAddress,
        CHandShake* hs);

    // Parameters:
    //    0) [in] u: the UDT socket ID.

    Result<std::shared_ptr<CUDT>> lookup(const UDTSOCKET u);

    // Parameters:
    //    0) [in] u: the UDT socket ID.
    // Returned value:
    //    UDT socket status, or NONEXIST if not found.

    UDTSTATUS getStatus(const UDTSOCKET u);

    // socket APIs

    Result<> bind(const UDTSOCKET u, const sockaddr* name, int namelen);
    Result<> bind(const UDTSOCKET u, UDPSOCKET udpsock);
    Result<> listen(const UDTSOCKET u, int backlog);
    Result<UDTSOCKET> accept(const UDTSOCKET listen, sockaddr* addr, int* addrlen);
    Result<> connect(const UDTSOCKET u, const sockaddr* name, int namelen);
    Result<> shutdown(const UDTSOCKET u, int how);
    Result<> close(const UDTSOCKET u);
    Result<> getpeername(const UDTSOCKET u, sockaddr* name, int* namelen);
    Result<> getsockname(const UDTSOCKET u, sockaddr* name, int* namelen);

    /**
     * @return Signalled socket count.
     */
    Result<int> select(ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout);

    /**
     * @return Signalled socket count.
     */
    Result<int> selectEx(
        const std::vector<UDTSOCKET>& fds,
        std::vector<UDTSOCKET>* readfds,
        std::vector<UDTSOCKET>* writefds,
        std::vector<UDTSOCKET>* exceptfds,
        int64_t msTimeOut);

    /**
     * @return epoll id.
     */
    Result<int> epoll_create();

    Result<> epoll_release(const int eid);

    Result<> epoll_add_usock(const int eid, const UDTSOCKET u, const int* events = NULL);
    Result<> epoll_add_ssock(const int eid, const SYSSOCKET s, const int* events = NULL);
    Result<> epoll_remove_usock(const int eid, const UDTSOCKET u);
    Result<> epoll_remove_ssock(const int eid, const SYSSOCKET s);

    /**
     * @return Signalled socket count.
     */
    Result<int> epoll_wait(
        const int eid,
        std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds, int64_t msTimeOut,
        std::map<SYSSOCKET, int>* lrfds = NULL, std::map<SYSSOCKET, int>* lwfds = NULL);

    Result<> epoll_interrupt_wait(int eid);

    /**
     * Set last error for the current thread.
     */
    void setError(Error e);

    /**
     * @return The last error occured in the current thread.
     */
    const Error& getError() const;

private:
    //   void init();

private:
    std::map<UDTSOCKET, std::shared_ptr<CUDTSocket>> m_Sockets;       // stores all the socket structures

    std::mutex m_ControlLock;                    // used to synchronize UDT API

    std::mutex m_IDLock;                         // used to synchronize ID generation
    UDTSOCKET m_SocketId = 0;                             // seed to generate a new unique socket ID

    std::map<int64_t, std::set<UDTSOCKET> > m_PeerRec;// record sockets from peers to avoid repeated connection request, int64_t = (socker_id << 30) + isn

private:
    std::map<ThreadId, Error> m_mTLSRecord;
    void cleanupPerThreadLastErrors();
    mutable std::mutex m_TLSLock;

private:
    Result<> connect_complete(const UDTSOCKET u);

    std::shared_ptr<CUDTSocket> locate(const UDTSOCKET u);

    std::shared_ptr<CUDTSocket> locate(
        const detail::SocketAddress& peer, const UDTSOCKET id, int32_t isn);

    Result<> updateMux(
        CUDTSocket* s,
        const std::optional<detail::SocketAddress>& addr = std::nullopt,
        const UDPSOCKET* = NULL);

    Result<> updateMux(CUDTSocket* s, const CUDTSocket* ls);

private:
    std::map<int, std::shared_ptr<Multiplexer>> m_multiplexers;

private:
    std::unique_ptr<CCache<CInfoBlock>> m_cache;            // UDT network information cache

private:
    volatile bool m_bClosing = false;
    std::mutex m_GCStopLock;
    std::condition_variable m_GCStopCond;

    std::mutex m_InitLock;
    int m_iInstanceCount = 0;                // number of startup() called by application
    bool m_bGCStatus = false;                    // if the GC thread is working (true)

    std::thread m_GCThread;

    void garbageCollect();

    std::map<UDTSOCKET, std::shared_ptr<CUDTSocket>> m_ClosedSockets;   // temporarily store closed sockets

    void checkBrokenSockets();
    void removeSocket(
        const UDTSOCKET u,
        std::vector<std::shared_ptr<Multiplexer>>* const multiplexersToRemove);

private:
    CEPoll m_EPoll;                                     // handling epoll data structures and events

private:
    CUDTUnited(const CUDTUnited&);
    CUDTUnited& operator=(const CUDTUnited&);
};

#endif
