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
Yunhong Gu, last updated 05/05/2011
*****************************************************************************/

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef LEGACY_WIN32
#include <wspiapi.h>
#endif
#endif
#include <cstring>
#include <iostream>
#include <sstream>
#include <functional>

#include "common.h"
#include "core.h"
#include "queue.h"

using namespace std;

//#define DEBUG_RECORD_PACKET_HISTORY

#ifdef DEBUG_RECORD_PACKET_HISTORY
namespace {

static std::string dumpPacket(const CPacket& packet)
{
    std::ostringstream ss;
    ss <<
        "id " << packet.m_iID << ", "
        "type " << (int)packet.getType();
    return ss.str();
}

static void dumpAddr(std::ostream& os, const sockaddr* addr)
{
    if (addr->sa_family == AF_INET)
    {
        os <<
            std::hex << ntohl(((const sockaddr_in*)addr)->sin_addr.s_addr) << ":" <<
            std::dec << ntohs(((const sockaddr_in*)addr)->sin_port);
    }
}

static std::string dumpPacket(const CPacket& packet, const sockaddr* addr)
{
    std::ostringstream ss;
    ss <<
        "id " << packet.m_iID << ", "
        "type " << (int)packet.getType()<<", "
        "addr ";

    dumpAddr(ss, addr);

    return ss.str();
}

static std::string dumpAddr(const sockaddr* addr)
{
    std::ostringstream ss;
    dumpAddr(ss, addr);
    return ss.str();
}

class SendedPacketVerifier
{
public:
    void beforeSendingPacket(const CPacket& packet)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        auto p = m_idToPrevSendedPacketType.emplace(packet.m_iID, SocketContext());
        p.first->second.packets.emplace_back(packet.getFlag(), packet.getType());

        //if (packet.getType() == ControlPacketType::KeepAlive)
        //{
        //    std::cout<<"Sending "<<dumpPacket(packet)<<"\n";
        //}
    }

    void packetReceived(const CPacket& packet)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        auto p = m_idToPrevReceivedPacketType.emplace(packet.m_iID, SocketContext());
        p.first->second.packets.emplace_back(packet.getFlag(), packet.getType());

        //if (packet.getType() == ControlPacketType::KeepAlive)
        //{
        //    std::cout << "Received " << dumpPacket(packet) << "\n";
        //}
    }

private:
    struct SocketContext
    {
        std::vector<std::pair<PacketFlag, ControlPacketType>> packets;
    };

    std::map<int, SocketContext> m_idToPrevSendedPacketType;
    std::map<int, SocketContext> m_idToPrevReceivedPacketType;
    std::mutex m_mutex;
};

static SendedPacketVerifier packetVerifier;

} // namespace

#endif // DEBUG_RECORD_PACKET_HISTORY


CUnitQueue::CUnitQueue():
    m_pQEntry(nullptr),
    m_pCurrQueue(nullptr),
    m_pLastQueue(nullptr),
    m_iSize(0),
    m_iCount(0),
    m_iMSS(),
    m_iIPversion()
{
}

CUnitQueue::~CUnitQueue()
{
    CQEntry* p = m_pQEntry;

    while (p != nullptr)
    {
        delete[] p->m_pUnit;
        delete[] p->m_pBuffer;

        CQEntry* q = p;
        if (p == m_pLastQueue)
            p = nullptr;
        else
            p = p->next;
        delete q;
    }
}

int CUnitQueue::init(int size, int mss, int version)
{
    CQEntry* tempq = nullptr;
    CUnit* tempu = nullptr;
    char* tempb = nullptr;

    try
    {
        tempq = new CQEntry;
        tempu = new CUnit[size];
        tempb = new char[size * mss];
    }
    catch (...)
    {
        delete tempq;
        delete[] tempu;
        delete[] tempb;

        return -1;
    }

    for (int i = 0; i < size; ++i)
    {
        tempu[i].m_iFlag = 0;
        tempu[i].m_Packet.m_pcData = tempb + i * mss;
    }
    tempq->m_pUnit = tempu;
    tempq->m_pBuffer = tempb;
    tempq->m_iSize = size;

    m_pQEntry = m_pCurrQueue = m_pLastQueue = tempq;
    m_pQEntry->next = m_pQEntry;

    m_pAvailUnit = m_pCurrQueue->m_pUnit;

    m_iSize = size;
    m_iMSS = mss;
    m_iIPversion = version;

    return 0;
}

int CUnitQueue::increase()
{
    // adjust/correct m_iCount
    int real_count = 0;
    CQEntry* p = m_pQEntry;
    while (p != nullptr)
    {
        CUnit* u = p->m_pUnit;
        for (CUnit* end = u + p->m_iSize; u != end; ++u)
            if (u->m_iFlag != 0)
                ++real_count;

        if (p == m_pLastQueue)
            p = nullptr;
        else
            p = p->next;
    }
    m_iCount = real_count;
    if (double(m_iCount) / m_iSize < 0.9)
        return -1;

    CQEntry* tempq = nullptr;
    CUnit* tempu = nullptr;
    char* tempb = nullptr;

    // all queues have the same size
    int size = m_pQEntry->m_iSize;

    try
    {
        tempq = new CQEntry;
        tempu = new CUnit[size];
        tempb = new char[size * m_iMSS];
    }
    catch (...)
    {
        delete tempq;
        delete[] tempu;
        delete[] tempb;

        return -1;
    }

    for (int i = 0; i < size; ++i)
    {
        tempu[i].m_iFlag = 0;
        tempu[i].m_Packet.m_pcData = tempb + i * m_iMSS;
    }
    tempq->m_pUnit = tempu;
    tempq->m_pBuffer = tempb;
    tempq->m_iSize = size;

    m_pLastQueue->next = tempq;
    m_pLastQueue = tempq;
    m_pLastQueue->next = m_pQEntry;

    m_iSize += size;

    return 0;
}

int CUnitQueue::shrink()
{
    // currently queue cannot be shrunk.
    return -1;
}

CUnit* CUnitQueue::getNextAvailUnit()
{
    if (m_iCount * 10 > m_iSize * 9)
        increase();

    if (m_iCount >= m_iSize)
        return nullptr;

    CQEntry* entrance = m_pCurrQueue;

    do
    {
        for (CUnit* sentinel = m_pCurrQueue->m_pUnit + m_pCurrQueue->m_iSize - 1; m_pAvailUnit != sentinel; ++m_pAvailUnit)
            if (m_pAvailUnit->m_iFlag == 0)
                return m_pAvailUnit;

        if (m_pCurrQueue->m_pUnit->m_iFlag == 0)
        {
            m_pAvailUnit = m_pCurrQueue->m_pUnit;
            return m_pAvailUnit;
        }

        m_pCurrQueue = m_pCurrQueue->next;
        m_pAvailUnit = m_pCurrQueue->m_pUnit;
    } while (m_pCurrQueue != entrance);

    increase();

    return nullptr;
}


CSndUList::CSndUList(
    CTimer* timer,
    std::mutex* windowLock,
    std::condition_variable* windowCond)
    :
    m_iArrayLength(4096),
    m_iLastEntry(-1),
    m_pWindowLock(windowLock),
    m_pWindowCond(windowCond),
    m_pTimer(timer)
{
    m_pHeap = new CSNode*[m_iArrayLength];
}

CSndUList::~CSndUList()
{
    delete[] m_pHeap;
}

void CSndUList::update(std::shared_ptr<CUDT> u, bool reschedule)
{
    std::lock_guard<std::mutex> listguard(m_ListLock);

    CSNode* n = u->sNode();

    if (n->locationOnHeap >= 0)
    {
        if (!reschedule)
            return;

        if (n->locationOnHeap == 0)
        {
            n->timestamp = 1;
            m_pTimer->interrupt();
            return;
        }

        remove_(u.get());
    }

    insert_(1, u);
}

int CSndUList::pop(sockaddr*& addr, CPacket& pkt)
{
    std::lock_guard<std::mutex> listguard(m_ListLock);

    if (-1 == m_iLastEntry)
        return -1;

    // no pop until the next schedulled time
    uint64_t ts;
    CTimer::rdtsc(ts);
    if (ts < m_pHeap[0]->timestamp)
        return -1;

    std::shared_ptr<CUDT> u = m_pHeap[0]->socket.lock();
    remove_(u.get());

    if (!u->connected() || u->broken())
        return -1;

    // pack a packet from the socket
    if (u->packData(pkt, ts) <= 0)
        return -1;

    addr = u->peerAddr();

    // insert a new entry, ts is the next processing time
    if (ts > 0)
        insert_(ts, u);

    return 1;
}

void CSndUList::remove(CUDT* u)
{
    std::lock_guard<std::mutex> listguard(m_ListLock);

    remove_(u);
}

uint64_t CSndUList::getNextProcTime()
{
    std::lock_guard<std::mutex> listguard(m_ListLock);

    if (-1 == m_iLastEntry)
        return 0;

    return m_pHeap[0]->timestamp;
}

void CSndUList::insert_(int64_t ts, std::shared_ptr<CUDT> u)
{
    CSNode* n = u->sNode();

    // do not insert repeated node
    if (n->locationOnHeap >= 0)
        return;

    m_iLastEntry++;
    m_pHeap[m_iLastEntry] = n;
    n->timestamp = ts;

    int q = m_iLastEntry;
    int p = q;
    while (p != 0)
    {
        p = (q - 1) >> 1;
        if (m_pHeap[p]->timestamp > m_pHeap[q]->timestamp)
        {
            CSNode* t = m_pHeap[p];
            m_pHeap[p] = m_pHeap[q];
            m_pHeap[q] = t;
            t->locationOnHeap = q;
            q = p;
        }
        else
            break;
    }

    n->locationOnHeap = q;

    // an earlier event has been inserted, wake up sending worker
    if (n->locationOnHeap == 0)
        m_pTimer->interrupt();

    // first entry, activate the sending queue
    if (0 == m_iLastEntry)
    {
        std::lock_guard<std::mutex> lock(*m_pWindowLock);
        m_pWindowCond->notify_all();
    }
}

void CSndUList::remove_(CUDT* u)
{
    CSNode* n = u->sNode();

    if (n->locationOnHeap >= 0)
    {
        // remove the node from heap
        m_pHeap[n->locationOnHeap] = m_pHeap[m_iLastEntry];
        m_iLastEntry--;
        m_pHeap[n->locationOnHeap]->locationOnHeap = n->locationOnHeap;

        int q = n->locationOnHeap;
        int p = q * 2 + 1;
        while (p <= m_iLastEntry)
        {
            if ((p + 1 <= m_iLastEntry) && (m_pHeap[p]->timestamp > m_pHeap[p + 1]->timestamp))
                p++;

            if (m_pHeap[q]->timestamp > m_pHeap[p]->timestamp)
            {
                CSNode* t = m_pHeap[p];
                m_pHeap[p] = m_pHeap[q];
                m_pHeap[p]->locationOnHeap = p;
                m_pHeap[q] = t;
                m_pHeap[q]->locationOnHeap = q;

                q = p;
                p = q * 2 + 1;
            }
            else
                break;
        }

        m_pHeap[m_iLastEntry + 1] = 0;
        n->locationOnHeap = -1;
    }

    // the only event has been deleted, wake up immediately
    if (0 == m_iLastEntry)
        m_pTimer->interrupt();
}

//
CSndQueue::CSndQueue(CChannel* c, CTimer* t):
    m_pSndUList(std::make_unique<CSndUList>(t, &m_WindowLock, &m_WindowCond)),
    m_pChannel(c),
    m_pTimer(t)
{
}

CSndQueue::~CSndQueue()
{
    m_bClosing = true;

    {
        std::lock_guard<std::mutex> lock(m_WindowLock);
        m_WindowCond.notify_all();
    }

    if (m_WorkerThread.joinable())
        m_WorkerThread.join();

    m_pSndUList.reset();
}

void CSndQueue::start()
{
    // TODO: #ak std::thread can throw. Convert exception to CUDTException(3, 1)?
    m_WorkerThread = std::thread([this]() { worker(); });
}

void CSndQueue::worker()
{
    while (!m_bClosing)
    {
        uint64_t ts = m_pSndUList->getNextProcTime();

        if (ts > 0)
        {
            // wait until next processing time of the first socket on the list
            uint64_t currtime;
            CTimer::rdtsc(currtime);
            if (currtime < ts)
                m_pTimer->sleepto(ts);

            // it is time to send the next pkt
            sockaddr* addr;
            CPacket pkt;
            if (m_pSndUList->pop(addr, pkt) < 0)
                continue;

#ifdef DEBUG_RECORD_PACKET_HISTORY
            packetVerifier.beforeSendingPacket(pkt);
#endif // DEBUG_RECORD_PACKET_HISTORY

            m_pChannel->sendto(addr, pkt);
        }
        else
        {
            // wait here if there is no sockets with data to be sent
            std::unique_lock<std::mutex> lock(m_WindowLock);
            m_WindowCond.wait(
                lock,
                [this]() { return m_bClosing || m_pSndUList->lastEntry() >= 0; });
        }
    }
}

int CSndQueue::sendto(const sockaddr* addr, CPacket& packet)
{
#ifdef DEBUG_RECORD_PACKET_HISTORY
    packetVerifier.beforeSendingPacket(packet);
#endif // DEBUG_RECORD_PACKET_HISTORY

    // send out the packet immediately (high priority), this is a control packet
    m_pChannel->sendto(addr, packet);
    return packet.getLength();
}

//-------------------------------------------------------------------------------------------------
// CRcvUList

void CRcvUList::insert(std::shared_ptr<CUDT> u)
{
    CRNode* n = u->rNode();
    CTimer::rdtsc(n->timestamp);

    if (!m_pUList)
    {
        // empty list, insert as the single node
        n->prev = n->next = nullptr;
        m_pLast = m_pUList = n;
        return;
    }

    // always insert at the end for RcvUList
    n->prev = m_pLast;
    n->next = nullptr;
    m_pLast->next = n;
    m_pLast = n;
}

void CRcvUList::remove(std::shared_ptr<CUDT> u)
{
    CRNode* n = u->rNode();

    if (!n->onList)
        return;

    if (nullptr == n->prev)
    {
        // n is the first node
        m_pUList = n->next;
        if (nullptr == m_pUList)
            m_pLast = nullptr;
        else
            m_pUList->prev = nullptr;
    }
    else
    {
        n->prev->next = n->next;
        if (nullptr == n->next)
        {
            // n is the last node
            m_pLast = n->prev;
        }
        else
            n->next->prev = n->prev;
    }

    n->next = n->prev = nullptr;
}

void CRcvUList::update(std::shared_ptr<CUDT> u)
{
    CRNode* n = u->rNode();

    if (!n->onList)
        return;

    CTimer::rdtsc(n->timestamp);

    // if n is the last node, do not need to change
    if (nullptr == n->next)
        return;

    if (nullptr == n->prev)
    {
        m_pUList = n->next;
        m_pUList->prev = nullptr;
    }
    else
    {
        n->prev->next = n->next;
        n->next->prev = n->prev;
    }

    n->prev = m_pLast;
    n->next = nullptr;
    m_pLast->next = n;
    m_pLast = n;
}

//-------------------------------------------------------------------------------------------------
// SocketByIdDict

std::shared_ptr<CUDT> SocketByIdDict::lookup(int32_t id)
{
    auto it = m_idToSocket.find(id);
    return it != m_idToSocket.end()
        ? it->second.lock()
        : nullptr;
}

void SocketByIdDict::insert(int32_t id, const std::weak_ptr<CUDT>& u)
{
    m_idToSocket.emplace(id, u);
}

void SocketByIdDict::remove(int32_t id)
{
    m_idToSocket.erase(id);
}

//-------------------------------------------------------------------------------------------------
// CRendezvousQueue

CRendezvousQueue::CRendezvousQueue()
{
}

CRendezvousQueue::~CRendezvousQueue()
{
    for (auto i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++i)
    {
        if (AF_INET == i->m_iIPversion)
            delete (sockaddr_in*)i->m_pPeerAddr;
        else
            delete (sockaddr_in6*)i->m_pPeerAddr;
    }

    m_lRendezvousID.clear();
}

void CRendezvousQueue::insert(
    const UDTSOCKET& id,
    std::weak_ptr<CUDT> u,
    int ipVersion,
    const sockaddr* addr,
    uint64_t ttl)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    CRL r;
    r.m_iID = id;
    r.socket = u;
    r.m_iIPversion = ipVersion;
    r.m_pPeerAddr = (AF_INET == ipVersion)
        ? (sockaddr*)new sockaddr_in 
        : (sockaddr*)new sockaddr_in6;
    memcpy(
        r.m_pPeerAddr,
        addr,
        (AF_INET == ipVersion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));
    r.m_ullTTL = ttl;

    m_lRendezvousID.push_back(r);
}

void CRendezvousQueue::remove(const UDTSOCKET& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++i)
    {
        if (i->m_iID == id)
        {
            if (AF_INET == i->m_iIPversion)
                delete (sockaddr_in*)i->m_pPeerAddr;
            else
                delete (sockaddr_in6*)i->m_pPeerAddr;

            m_lRendezvousID.erase(i);
            return;
        }
    }
}

std::shared_ptr<CUDT> CRendezvousQueue::getByAddr(
    const sockaddr* addr,
    UDTSOCKET& id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // TODO: optimize search
    for (const auto& crl: m_lRendezvousID)
    {
        if (CIPAddress::ipcmp(addr, crl.m_pPeerAddr, crl.m_iIPversion) && 
            (id == 0 || id == crl.m_iID))
        {
            id = crl.m_iID;
            return crl.socket.lock();
        }
    }

    return nullptr;
}

void CRendezvousQueue::updateConnStatus()
{
    if (m_lRendezvousID.empty())
        return;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++i)
    {
        auto udtSocket = i->socket.lock();
        if (!udtSocket)
            continue;

        // avoid sending too many requests, at most 1 request per 250ms
        if (CTimer::getTime() - udtSocket->lastReqTime() > 250000)
        {
            if (CTimer::getTime() >= i->m_ullTTL)
            {
                // connection timer expired, acknowledge app via epoll
                udtSocket->setConnecting(false);
                CUDT::s_UDTUnited.m_EPoll.update_events(
                    i->m_iID,
                    udtSocket->pollIds(),
                    UDT_EPOLL_ERR,
                    true);
                continue;
            }

            CPacket request;
            char* reqdata = new char[udtSocket->payloadSize()];
            request.pack(ControlPacketType::Handshake, nullptr, reqdata, udtSocket->payloadSize());
            // ID = 0, connection request
            request.m_iID = !udtSocket->rendezvous() ? 0 : udtSocket->connRes().m_iID;
            int hs_size = udtSocket->payloadSize();
            udtSocket->connReq().serialize(reqdata, hs_size);
            request.setLength(hs_size);
            udtSocket->sndQueue().sendto(i->m_pPeerAddr, request);
            udtSocket->setLastReqTime(CTimer::getTime());
            delete[] reqdata;
        }
    }
}

//-------------------------------------------------------------------------------------------------
// CRcvQueue

CRcvQueue::CRcvQueue(
    int size,
    int payload,
    int ipVersion,
    int hsize,
    CChannel* c,
    CTimer* t)
    :
    m_pRcvUList(std::make_unique<CRcvUList>()),
    m_pChannel(c),
    m_pTimer(t),
    m_iPayloadSize(payload),
    m_bClosing(false),
    m_pRendezvousQueue(std::make_unique<CRendezvousQueue>())
{
    m_iPayloadSize = payload;

    m_UnitQueue.init(size, payload, ipVersion);
}

CRcvQueue::~CRcvQueue()
{
    m_bClosing = true;

    if (m_WorkerThread.joinable())
        m_WorkerThread.join();

    m_pRcvUList.reset();
    m_pRendezvousQueue.reset();

    // remove all queued messages
    for (auto i = m_mBuffer.begin(); i != m_mBuffer.end(); ++i)
    {
        while (!i->second.empty())
        {
            CPacket* pkt = i->second.front();
            delete[] pkt->m_pcData;
            delete pkt;
            i->second.pop();
        }
    }
}

void CRcvQueue::start()
{
    m_WorkerThread = std::thread(&CRcvQueue::worker, this);
    // TODO: #ak Convert std::thread exception to CUDTException(3, 1)?
}

void CRcvQueue::stop()
{
    m_bClosing = true;

    if (m_WorkerThread.joinable())
        m_WorkerThread.join();
}

void CRcvQueue::worker()
{
    sockaddr* addr = (AF_INET == m_UnitQueue.m_iIPversion) ? (sockaddr*) new sockaddr_in : (sockaddr*) new sockaddr_in6;
    int32_t id;

    while (!m_bClosing)
    {
#ifdef NO_BUSY_WAITING
        m_pTimer->tick();
#endif

        // check waiting list, if new socket, insert it to the list
        while (!m_vNewEntry.empty())
        {
            std::shared_ptr<CUDT> ne = takeNewEntry();
            if (ne)
            {
                m_pRcvUList->insert(ne);
                m_socketByIdDict.insert(ne->socketId(), ne);
            }
        }

        // find next available slot for incoming packet
        CUnit* unit = m_UnitQueue.getNextAvailUnit();
        if (nullptr == unit)
        {
            // no space, skip this packet
            CPacket temp;
            temp.m_pcData = new char[m_iPayloadSize];
            temp.setLength(m_iPayloadSize);
            m_pChannel->recvfrom(addr, temp);
            delete[] temp.m_pcData;
            goto TIMER_CHECK;
        }

        unit->m_Packet.setLength(m_iPayloadSize);

        // reading next incoming packet, recvfrom returns -1 is nothing has been received
        if (m_pChannel->recvfrom(addr, unit->m_Packet) < 0)
            goto TIMER_CHECK;

        id = unit->m_Packet.m_iID;

        try
        {
            // ID 0 is for connection request, which should be passed to the listening socket or rendezvous sockets
            if (0 == id)
            {
                if (auto listener = m_listener.lock())
                {
                    listener->processConnectionRequest(addr, unit->m_Packet);
                }
                else if (auto u = m_pRendezvousQueue->getByAddr(addr, id))
                {
                    // asynchronous connect: call connect here
                    // otherwise wait for the UDT socket to retrieve this packet
                    if (!u->synRecving())
                        u->connect(unit->m_Packet);
                    else
                        storePkt(id, unit->m_Packet.clone());
                }
            }
            else if (id > 0)
            {
#ifdef DEBUG_RECORD_PACKET_HISTORY
                packetVerifier.packetReceived(unit->m_Packet);
#endif // DEBUG_RECORD_PACKET_HISTORY

                if (auto u = m_socketByIdDict.lookup(id))
                {
                    if (CIPAddress::ipcmp(addr, u->peerAddr(), u->ipVersion()))
                    {
                        if (u->connected() && !u->broken() && !u->isClosing())
                        {
                            if (unit->m_Packet.getFlag() == PacketFlag::Data)
                                u->processData(unit);
                            else
                                u->processCtrl(unit->m_Packet);

                            u->checkTimers(false);
                            m_pRcvUList->update(u);
                        }
                    }
                }
                else if (auto u = m_pRendezvousQueue->getByAddr(addr, id))
                {
                    if (!u->synRecving())
                        u->connect(unit->m_Packet);
                    else
                        storePkt(id, unit->m_Packet.clone());
                }
            }
        }
        catch (CUDTException /*e*/)
        {
            //socket has been removed before connect finished? ignoring error...
        }

    TIMER_CHECK:
        // take care of the timing event for all UDT sockets

        uint64_t currtime;
        CTimer::rdtsc(currtime);

        CRNode* ul = m_pRcvUList->m_pUList;
        uint64_t ctime = currtime - 100000 * CTimer::getCPUFrequency();
        while ((nullptr != ul) && (ul->timestamp < ctime))
        {
            std::shared_ptr<CUDT> u = ul->socket.lock();

            if (u->connected() && !u->broken() && !u->isClosing())
            {
                u->checkTimers(false);
                m_pRcvUList->update(u);
            }
            else
            {
                // the socket must be removed from Hash table first, then RcvUList
                m_socketByIdDict.remove(u->socketId());
                m_pRcvUList->remove(u);
                u->rNode()->onList = false;
            }

            ul = m_pRcvUList->m_pUList;
        }

        // Check connection requests status for all sockets in the RendezvousQueue.
        m_pRendezvousQueue->updateConnStatus();
    }

    if (AF_INET == m_UnitQueue.m_iIPversion)
        delete (sockaddr_in*)addr;
    else
        delete (sockaddr_in6*)addr;
}

int CRcvQueue::recvfrom(int32_t id, CPacket& packet)
{
    std::unique_lock<std::mutex> lock(m_PassLock);

    map<int32_t, std::queue<CPacket*> >::iterator i = m_mBuffer.find(id);

    if (i == m_mBuffer.end())
    {
        m_PassCond.wait_for(lock, std::chrono::seconds(1));

        i = m_mBuffer.find(id);
        if (i == m_mBuffer.end())
        {
            packet.setLength(-1);
            return -1;
        }
    }

    // retrieve the earliest packet
    CPacket* newpkt = i->second.front();

    if (packet.getLength() < newpkt->getLength())
    {
        packet.setLength(-1);
        return -1;
    }

    // copy packet content
    memcpy(packet.m_nHeader, newpkt->m_nHeader, CPacket::m_iPktHdrSize);
    memcpy(packet.m_pcData, newpkt->m_pcData, newpkt->getLength());
    packet.setLength(newpkt->getLength());

    delete[] newpkt->m_pcData;
    delete newpkt;

    // remove this message from queue, 
    // if no more messages left for this socket, release its data structure
    i->second.pop();
    if (i->second.empty())
        m_mBuffer.erase(i);

    return packet.getLength();
}

bool CRcvQueue::setListener(std::weak_ptr<ServerSideConnectionAcceptor> listener)
{
    std::lock_guard<std::mutex> lock(m_LSLock);

    if (m_listener.lock())
        return false;

    m_listener = listener;
    return true;
}

void CRcvQueue::registerConnector(
    const UDTSOCKET& id,
    std::shared_ptr<CUDT> u,
    int ipVersion,
    const sockaddr* addr,
    uint64_t ttl)
{
    m_pRendezvousQueue->insert(id, u, ipVersion, addr, ttl);
}

void CRcvQueue::removeConnector(const UDTSOCKET& id)
{
    m_pRendezvousQueue->remove(id);

    std::lock_guard<std::mutex> bufferlock(m_PassLock);

    map<int32_t, std::queue<CPacket*> >::iterator i = m_mBuffer.find(id);
    if (i != m_mBuffer.end())
    {
        while (!i->second.empty())
        {
            delete[] i->second.front()->m_pcData;
            delete i->second.front();
            i->second.pop();
        }
        m_mBuffer.erase(i);
    }
}

void CRcvQueue::addNewEntry(const std::weak_ptr<CUDT>& u)
{
    std::lock_guard<std::mutex> lock(m_IDLock);
    m_vNewEntry.push_back(u);
}

std::shared_ptr<CUDT> CRcvQueue::takeNewEntry()
{
    std::lock_guard<std::mutex> lock(m_IDLock);

    if (m_vNewEntry.empty())
        return nullptr;

    std::shared_ptr<CUDT> u = m_vNewEntry.front().lock();
    m_vNewEntry.erase(m_vNewEntry.begin());

    return u;
}

void CRcvQueue::storePkt(int32_t id, CPacket* pkt)
{
    std::lock_guard<std::mutex> bufferlock(m_PassLock);

    map<int32_t, std::queue<CPacket*> >::iterator i = m_mBuffer.find(id);

    if (i == m_mBuffer.end())
    {
        m_mBuffer[id].push(pkt);

        m_PassCond.notify_all();
    }
    else
    {
        //avoid storing too many packets, in case of malfunction or attack
        if (i->second.size() > 16)
            return;

        i->second.push(pkt);
    }
}
