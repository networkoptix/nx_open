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
Yunhong Gu, last updated 08/20/2010
*****************************************************************************/

#ifndef __UDT_EPOLL_H__
#define __UDT_EPOLL_H__


#include <map>
#include <memory>
#include <mutex>
#include <set>

#include "common.h"
#include "result.h"
#include "udt.h"


class EpollImpl;

class CEPoll
{
    friend class CUDT;
    friend class ServerSideConnectionAcceptor;
    friend class CRendezvousQueue;

public:
    CEPoll();
    ~CEPoll();

public: // for CUDTUnited API

        // Functionality:
        //    create a new EPoll.
        // Parameters:
        //    None.
        // Returned value:
        //    new EPoll ID.

    Result<int> create();

    // Functionality:
    //    add a UDT socket to an EPoll.
    // Parameters:
    //    0) [in] eid: EPoll ID.
    //    1) [in] u: UDT Socket ID.
    //    2) [in] events: events to watch.

    Result<> add_usock(const int eid, const UDTSOCKET& u, const int* events = NULL);

    // Functionality:
    //    add a system socket to an EPoll.
    // Parameters:
    //    0) [in] eid: EPoll ID.
    //    1) [in] s: system Socket ID.
    //    2) [in] events: events to watch.

    Result<> add_ssock(const int eid, const SYSSOCKET& s, const int* events = NULL);

    // Functionality:
    //    remove a UDT socket event from an EPoll; socket will be removed if no events to watch
    // Parameters:
    //    0) [in] eid: EPoll ID.
    //    1) [in] u: UDT socket ID.

    Result<> remove_usock(const int eid, const UDTSOCKET& u);

    // Functionality:
    //    remove a system socket event from an EPoll; socket will be removed if no events to watch
    // Parameters:
    //    0) [in] eid: EPoll ID.
    //    1) [in] s: system socket ID.

    Result<> remove_ssock(const int eid, const SYSSOCKET& s);

    // Functionality:
    //    wait for EPoll events or timeout.
    // Parameters:
    //    0) [in] eid: EPoll ID.
    //    1) [out] readfds: UDT sockets available for reading.
    //    2) [out] writefds: UDT sockets available for writing.
    //    3) [in] msTimeOut: timeout threshold, in milliseconds.
    //    4) [out] lrfds: system file descriptors for reading.
    //    5) [out] lwfds: system file descriptors for writing.
    // Returned value:
    //    number of sockets available for IO.

    Result<int> wait(
        const int eid,
        std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds, int64_t msTimeOut,
        std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds);

    // Functionality:
    //    Interrupt one wait call: the one running simultaneously
    //    in another thread or the next one to be made.
    // Parameters:
    //    0) [in] eid: EPoll ID.

    Result<> interruptWait(const int eid);

    // Functionality:
    //    close and release an EPoll.
    // Parameters:
    //    0) [in] eid: EPoll ID.

    Result<> release(const int eid);

    void RemoveEPollEvent(UDTSOCKET socket);

public: // for CUDT to acknowledge IO status

        // Functionality:
        //    Update events available for a UDT socket.
        // Parameters:
        //    0) [in] uid: UDT socket ID.
        //    1) [in] eids: EPoll IDs to be set
        //    1) [in] events: Combination of events to update
        //    1) [in] enable: true -> enable, otherwise disable
        // Returned value:
        //    0 if success, otherwise an error number

    int update_events(
        const UDTSOCKET& socketId,
        const std::set<int>& epollToTriggerIDs,
        int events,
        bool enable);

private:
    typedef std::map<int, std::unique_ptr<EpollImpl>> CEPollDescMap;

    int m_iIDSeed = 0;                            // seed to generate a new ID

    CEPollDescMap m_mPolls;       // all epolls
    mutable std::mutex m_EPollLock;

    Result<EpollImpl*> getEpollById(int eid) const;
};

#endif
