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

static constexpr auto kSyncRepeatMinPeriod = std::chrono::milliseconds(250);

class CUDT;
class CUnitQueue;

struct CUnit
{
    enum class Flag
    {
        free_ = 0,
        occupied,
        outOfOrder, //< msg read but not freed
        msgDropped,
    };

    // TODO: #ak There is a dependency loop between CUnitQueue and CUnit.
    CUnit(CUnitQueue* unitQueue);

    CPacket& packet();

    void setFlag(Flag val);
    Flag flag() const;

private:
    CPacket m_Packet;
    // 0: free, 1: occupied, 2: msg read but not freed (out-of-order), 3: msg dropped.
    Flag m_iFlag = Flag::free_;
    CUnitQueue* m_unitQueue = nullptr;
};

class ServerSideConnectionAcceptor;

class CUnitQueue
{
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

    int init(int size, int mss);

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

    int decCount() { return --m_iCount; }
    int incCount() { return ++m_iCount; }

private:
    struct CQEntry
    {
        // TODO: #ak It makes sense to use a single buffer of (size * m_iMSS) here.
        std::vector<CUnit> unitQueue;
        CQEntry* next = nullptr;

        CQEntry(std::vector<CUnit> unitQueue):
            unitQueue(std::move(unitQueue))
        {
        }
    };

    CQEntry* m_pQEntry = nullptr;            // pointer to the first unit queue
    CQEntry* m_pCurrQueue = nullptr;        // pointer to the current available queue
    CQEntry* m_pLastQueue = nullptr;        // pointer to the last unit queue

    CUnit* m_pAvailUnit = nullptr;         // recent available unit

    int m_iSize = 0;            // total size of the unit queue, in number of packets
    int m_iCount = 0;        // total number of valid packets in the queue

    int m_iMSS = 0;            // unit buffer size

private:
    std::unique_ptr<CQEntry> makeEntry(std::size_t size);

    CUnitQueue(const CUnitQueue&);
    CUnitQueue& operator=(const CUnitQueue&);
};

//-------------------------------------------------------------------------------------------------

struct CSNode
{
    std::weak_ptr<CUDT> socket;
    std::chrono::microseconds timestamp = std::chrono::microseconds::zero();

    // -1 means not on the heap.
    int locationOnHeap = -1;
};

class UDT_API CSndUList
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

    std::chrono::microseconds getNextProcTime() const;

    int lastEntry() const { return m_iLastEntry; }

private:
    void insert_(
        std::chrono::microseconds ts,
        CSNode* n);

    void remove_(CSNode* n);

private:
    // The heap array. CSNode objects are owned by m_socketToNode.
    std::vector<CSNode*> m_nodeHeap;
    std::map<CUDT*, std::unique_ptr<CSNode>> m_socketToNode;
    // physical length of the array
    int m_iArrayLength = 0;
    // position of last entry on the heap array
    int m_iLastEntry = 0;

    mutable std::mutex m_mutex;

    std::mutex* m_pWindowLock = nullptr;
    std::condition_variable* m_pWindowCond = nullptr;

    CTimer* m_timer = nullptr;

private:
    CSndUList(const CSndUList&);
    CSndUList& operator=(const CSndUList&);
};

//-------------------------------------------------------------------------------------------------

/**
 * List of sockets sorted by timestamp in ascending order.
 */
class CRcvUList
{
public:
    struct CRNode
    {
        UDTSOCKET socketId = -1;
        std::weak_ptr<CUDT> socket;
        std::chrono::microseconds timestamp = std::chrono::microseconds::zero();
    };

    using iterator = std::list<CRNode>::iterator;

    void push_back(std::shared_ptr<CUDT> u);
    void erase(iterator it);
    void sink(UDTSOCKET socketId);

    bool empty() const { return m_sockets.empty(); }
    iterator begin() { return m_sockets.begin(); }
    iterator end() { return m_sockets.end(); }

private:
    std::list<CRNode> m_sockets;
    std::map<UDTSOCKET, std::list<CRNode>::iterator> m_socketIdToNode;
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
        std::chrono::microseconds ttl);

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
        std::chrono::microseconds ttl = std::chrono::microseconds::zero();

        CRL();
    };

    std::list<CRL> m_rendezvousSockets;      // The sockets currently in rendezvous mode
    mutable std::mutex m_mutex;
};

//-------------------------------------------------------------------------------------------------

class CSndQueue
{
public:
    CSndQueue(AbstractUdpChannel* c, CTimer* t);
    ~CSndQueue();

    void start();

    // Functionality:
    //    Send out a packet to a given address.
    // Parameters:
    //    1) [in] addr: destination address
    //    2) [in] packet: packet to be sent out
    // Returned value:
    //    Size of data sent out.

    int sendto(const detail::SocketAddress& addr, CPacket packet);

    // List of UDT instances for data sending.
    CSndUList& sndUList() { return *m_pSndUList; }

    detail::SocketAddress getLocalAddr() const;

private:
    void worker();

    std::thread m_WorkerThread;

private:
    struct SendTask
    {
        detail::SocketAddress addr;
        CPacket packet;
    };

    // List of UDT instances for data sending
    std::unique_ptr<CSndUList> m_pSndUList;
    // The UDP channel for data sending
    AbstractUdpChannel* m_channel = nullptr;
    // Timing facility
    CTimer* m_timer = nullptr;
    std::vector<SendTask> m_sendTasks;

    std::mutex m_mutex;
    std::condition_variable m_cond;

    volatile bool m_bClosing = false;        // closing the worker

private:
    int sendPacket(const detail::SocketAddress& addr, CPacket packet);
    void postPacket(const detail::SocketAddress& addr, CPacket packet);
    void sendPostedPackets();

    CSndQueue(const CSndQueue&);
    CSndQueue& operator=(const CSndQueue&);
};

//-------------------------------------------------------------------------------------------------

class CRcvQueue
{
public:
    static constexpr auto kDefaultRecvTimeout = std::chrono::seconds(1);

    /**
     * Initialize the receiving queue.
     * @param size queue size
     * @param mss maximum packet size
     * @param c UDP channel to be associated to the queue
     * @param t timer
     */
    CRcvQueue(
        int size,
        int payload,
        int ipVersion,
        AbstractUdpChannel* c,
        CTimer* t);
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
    // TODO: #ak Refactor this function to return packet, not copy it.

    int recvfrom(
        int32_t id,
        CPacket& packet,
        std::chrono::microseconds timeout = kDefaultRecvTimeout);

    bool setListener(std::weak_ptr<ServerSideConnectionAcceptor> listener);

    void registerConnector(
        const UDTSOCKET& id,
        std::shared_ptr<CUDT> u,
        const detail::SocketAddress& addr,
        std::chrono::microseconds ttl);

    void removeConnector(const UDTSOCKET& id);

    void addNewEntry(const std::weak_ptr<CUDT>& u);

private:
    std::shared_ptr<CUDT> takeNewEntry();

    void worker();

    /**
     * Take care of the timing event for all UDT sockets.
     */
    void timerCheck();

    Result<> processUnit(CUnit* unit, const detail::SocketAddress& addr);

    std::thread m_WorkerThread;

private:
    CUnitQueue m_UnitQueue;        // The received packet queue

    // List of UDT instances that will read packets from the queue.
    CRcvUList m_rcvUList;
    // Hash table for UDT socket looking up
    SocketByIdDict m_socketByIdDict;
    // UDP channel for receving packets
    AbstractUdpChannel* m_channel = nullptr;
    // shared timer with the snd queue
    CTimer* m_timer = nullptr;
    const int m_iIPversion;

    // packet payload size
    int m_iPayloadSize = 0;

    // closing the workder
    volatile bool m_bClosing = false;

private:
    void storePkt(int32_t id, std::unique_ptr<CPacket> pkt);

private:
    // pointer to the (unique, if any) listening UDT entity
    std::weak_ptr<ServerSideConnectionAcceptor> m_listener;
    // The list of sockets in rendezvous mode
    std::unique_ptr<CRendezvousQueue> m_pRendezvousQueue;

    // newly added entries, to be inserted
    std::vector<std::weak_ptr<CUDT>> m_vNewEntry;
    std::mutex m_mutex;
    std::condition_variable m_cond;

    // temporary buffer for rendezvous connection request
    std::map<int32_t, std::queue<std::unique_ptr<CPacket>>> m_packets;

private:
    CRcvQueue(const CRcvQueue&);
    CRcvQueue& operator=(const CRcvQueue&);
};

#endif
