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
Yunhong Gu, last updated 01/12/2011
*****************************************************************************/


#ifndef __UDT_QUEUE_H__
#define __UDT_QUEUE_H__

#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include <thread>

#include "channel.h"
#include "common.h"
#include "packet.h"

class CUDT;

struct CUnit
{
    CPacket m_Packet;        // packet
    int m_iFlag;            // 0: free, 1: occupied, 2: msg read but not freed (out-of-order), 3: msg dropped
};

class ServerSideConnectionAcceptor;

class CUnitQueue
{
    friend class CRcvQueue;
    friend class CRcvBuffer;

public:
    CUnitQueue();
    ~CUnitQueue();

public:

    // Functionality:
    //    Initialize the unit queue.
    // Parameters:
    //    1) [in] size: queue size
    //    2) [in] mss: maximum segament size
    //    3) [in] version: IP version
    // Returned value:
    //    0: success, -1: failure.

    int init(int size, int mss, int version);

    // Functionality:
    //    Increase (double) the unit queue size.
    // Parameters:
    //    None.
    // Returned value:
    //    0: success, -1: failure.

    int increase();

    // Functionality:
    //    Decrease (halve) the unit queue size.
    // Parameters:
    //    None.
    // Returned value:
    //    0: success, -1: failure.

    int shrink();

    // Functionality:
    //    find an available unit for incoming packet.
    // Parameters:
    //    None.
    // Returned value:
    //    Pointer to the available unit, NULL if not found.

    CUnit* getNextAvailUnit();

private:
    struct CQEntry
    {
        CUnit* m_pUnit;        // unit queue
        char* m_pBuffer;        // data buffer
        int m_iSize;        // size of each queue

        CQEntry* next;
    }
    *m_pQEntry,            // pointer to the first unit queue
        *m_pCurrQueue,        // pointer to the current available queue
        *m_pLastQueue;        // pointer to the last unit queue

    CUnit* m_pAvailUnit;         // recent available unit

    int m_iSize;            // total size of the unit queue, in number of packets
    int m_iCount;        // total number of valid packets in the queue

    int m_iMSS;            // unit buffer size
    int m_iIPversion;        // IP version

private:
    CUnitQueue(const CUnitQueue&);
    CUnitQueue& operator=(const CUnitQueue&);
};

//-------------------------------------------------------------------------------------------------

struct CSNode
{
    std::weak_ptr<CUDT> socket;
    uint64_t timestamp = 0;

    // -1 means not on the heap.
    int locationOnHeap = -1;
};

class CSndUList
{
public:
    CSndUList(
        CTimer* timer,
        std::mutex* windowLock,
        std::condition_variable* windowCond);
    ~CSndUList();

public:

    // Functionality:
    //    Update the timestamp of the UDT instance on the list.
    // Parameters:
    //    1) [in] u: pointer to the UDT instance
    //    2) [in] resechedule: if the timestamp should be rescheduled
    // Returned value:
    //    None.

    void update(std::shared_ptr<CUDT> u, bool reschedule = true);

    // Functionality:
    //    Retrieve the next packet and peer address from the first entry, and reschedule it in the queue.
    // Parameters:
    //    0) [out] addr: destination address of the next packet
    //    1) [out] pkt: the next packet to be sent
    // Returned value:
    //    1 if successfully retrieved, -1 if no packet found.

    int pop(detail::SocketAddress& addr, CPacket& pkt);

    // Functionality:
    //    Remove UDT instance from the list.
    // Parameters:
    //    1) [in] u: pointer to the UDT instance
    // Returned value:
    //    None.

    void remove(CUDT* u);

    // Functionality:
    //    Retrieve the next scheduled processing time.
    // Parameters:
    //    None.
    // Returned value:
    //    Scheduled processing time of the first UDT socket in the list.

    uint64_t getNextProcTime();

    int lastEntry() const { return m_iLastEntry; }

private:
    void insert_(int64_t ts, std::shared_ptr<CUDT> u);
    void remove_(CSNode* n);

private:
    // The heap array
    std::vector<CSNode*> m_nodeHeap;
    // physical length of the array
    int m_iArrayLength = 0;
    // position of last entry on the heap array
    int m_iLastEntry = 0;

    std::mutex m_ListLock;

    std::mutex* m_pWindowLock = nullptr;
    std::condition_variable* m_pWindowCond = nullptr;

    CTimer* m_timer = nullptr;

private:
    CSndUList(const CSndUList&);
    CSndUList& operator=(const CSndUList&);
};

//-------------------------------------------------------------------------------------------------

struct CRNode
{
    UDTSOCKET socketId = -1;
    std::weak_ptr<CUDT> socket;
    uint64_t timestamp = 0;

    CRNode* prev = nullptr;
    CRNode* next = nullptr;
};

class CRcvUList
{
public:
    CRcvUList() = default;
    ~CRcvUList();

    CRcvUList(const CRcvUList&) = delete;
    CRcvUList& operator=(const CRcvUList&) = delete;

    // Functionality:
    //    Insert a new UDT instance to the list.
    // Parameters:
    //    1) [in] u: pointer to the UDT instance
    // Returned value:
    //    None.

    void insert(std::shared_ptr<CUDT> u);

    // Functionality:
    //    Remove the UDT instance from the list.
    // Returned value:
    //    None.

    void remove(CRNode* node);

    // Functionality:
    //    Move the UDT instance to the end of the list, if it already exists; otherwise, do nothing.
    // Returned value:
    //    None.

    void update(UDTSOCKET socketId);

public:
    // TODO: #ak Replace this double-linked list implementation with std container.
    CRNode* m_nodeListHead = nullptr;        // the head node

private:
    CRNode* m_nodeListTail = nullptr;        // the last node
    std::map<UDTSOCKET, CRNode*> m_socketIdToNode;
};

//-------------------------------------------------------------------------------------------------

class SocketByIdDict
{
public:
    std::shared_ptr<CUDT> lookup(int32_t id);

    void insert(int32_t id, const std::weak_ptr<CUDT>& u);

    void remove(int32_t id);

private:
    std::map<int32_t, std::weak_ptr<CUDT>> m_idToSocket;
};

//-------------------------------------------------------------------------------------------------

class CRendezvousQueue
{
public:
    CRendezvousQueue();
    ~CRendezvousQueue();

public:
    void insert(
        const UDTSOCKET& id,
        std::weak_ptr<CUDT> u,
        const detail::SocketAddress& addr,
        uint64_t ttl);

    void remove(const UDTSOCKET& id);

    std::shared_ptr<CUDT> getByAddr(
        const detail::SocketAddress& addr,
        UDTSOCKET& id) const;

    void updateConnStatus();

private:
    struct CRL
    {
        UDTSOCKET id = -1;
        std::weak_ptr<CUDT> socket;
        // UDT sonnection peer address
        detail::SocketAddress peerAddr;
        // the time that this request expires
        uint64_t ttl = 0;

        CRL();
    };

    std::list<CRL> m_rendezvousSockets;      // The sockets currently in rendezvous mode
    mutable std::mutex m_mutex;
};

//-------------------------------------------------------------------------------------------------

class CSndQueue
{
public:
    CSndQueue(UdpChannel* c, CTimer* t);
    ~CSndQueue();

    void start();

    // Functionality:
    //    Send out a packet to a given address.
    // Parameters:
    //    1) [in] addr: destination address
    //    2) [in] packet: packet to be sent out
    // Returned value:
    //    Size of data sent out.

    int sendto(const detail::SocketAddress& addr, CPacket& packet);

    // List of UDT instances for data sending.
    CSndUList& sndUList() { return *m_pSndUList; }

    // The UDP channel for data sending.
    UdpChannel* channel() { return m_channel; }

private:
    void worker();

    std::thread m_WorkerThread;

private:
    // List of UDT instances for data sending
    std::unique_ptr<CSndUList> m_pSndUList;
    // The UDP channel for data sending
    UdpChannel* m_channel = nullptr;
    // Timing facility
    CTimer* m_timer = nullptr;

    std::mutex m_WindowLock;
    std::condition_variable m_WindowCond;

    volatile bool m_bClosing = false;        // closing the worker

private:
    CSndQueue(const CSndQueue&);
    CSndQueue& operator=(const CSndQueue&);
};

//-------------------------------------------------------------------------------------------------

class CRcvQueue
{
public:
    /**
     * Initialize the receiving queue.
     * @param size queue size
     * @param mss maximum packet size
     * @param c UDP channel to be associated to the queue
     * @param t timer
     */
    CRcvQueue(int size, int payload, int ipVersion, UdpChannel* c, CTimer* t);
    ~CRcvQueue();

    void start();
    void stop();

public:

    // Functionality:
    //    Read a packet for a specific UDT socket id.
    // Parameters:
    //    1) [in] id: Socket ID
    //    2) [out] packet: received packet
    // Returned value:
    //    Data size of the packet

    int recvfrom(int32_t id, CPacket& packet);

    bool setListener(std::weak_ptr<ServerSideConnectionAcceptor> listener);

    void registerConnector(
        const UDTSOCKET& id,
        std::shared_ptr<CUDT> u,
        const detail::SocketAddress& addr,
        uint64_t ttl);

    void removeConnector(const UDTSOCKET& id);

    void addNewEntry(const std::weak_ptr<CUDT>& u);
    std::shared_ptr<CUDT> takeNewEntry();

    // The received packet queue.
    CUnitQueue* unitQueue() { return &m_UnitQueue; }

private:
    void worker();

    std::thread m_WorkerThread;

private:
    CUnitQueue m_UnitQueue;        // The received packet queue

    // List of UDT instances that will read packets from the queue.
    std::unique_ptr<CRcvUList> m_pRcvUList;
    // Hash table for UDT socket looking up
    SocketByIdDict m_socketByIdDict;
    // UDP channel for receving packets
    UdpChannel* m_channel = nullptr;
    // shared timer with the snd queue
    CTimer* m_timer = nullptr;

    // packet payload size
    int m_iPayloadSize = 0;

    // closing the workder
    volatile bool m_bClosing = false;

private:
    void storePkt(int32_t id, CPacket* pkt);

private:
    std::mutex m_LSLock;
    // pointer to the (unique, if any) listening UDT entity
    std::weak_ptr<ServerSideConnectionAcceptor> m_listener;
    // The list of sockets in rendezvous mode
    std::unique_ptr<CRendezvousQueue> m_pRendezvousQueue;

    // newly added entries, to be inserted
    std::vector<std::weak_ptr<CUDT>> m_vNewEntry;
    std::mutex m_IDLock;

    // temporary buffer for rendezvous connection request
    std::map<int32_t, std::queue<CPacket*> > m_mBuffer;
    std::mutex m_PassLock;
    std::condition_variable m_PassCond;

private:
    CRcvQueue(const CRcvQueue&);
    CRcvQueue& operator=(const CRcvQueue&);
};

#endif
