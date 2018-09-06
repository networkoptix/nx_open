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
    m_pQEntry(NULL),
    m_pCurrQueue(NULL),
    m_pLastQueue(NULL),
    m_iSize(0),
    m_iCount(0),
    m_iMSS(),
    m_iIPversion()
{
}

CUnitQueue::~CUnitQueue()
{
    CQEntry* p = m_pQEntry;

    while (p != NULL)
    {
        delete[] p->m_pUnit;
        delete[] p->m_pBuffer;

        CQEntry* q = p;
        if (p == m_pLastQueue)
            p = NULL;
        else
            p = p->m_pNext;
        delete q;
    }
}

int CUnitQueue::init(int size, int mss, int version)
{
    CQEntry* tempq = NULL;
    CUnit* tempu = NULL;
    char* tempb = NULL;

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
    m_pQEntry->m_pNext = m_pQEntry;

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
    while (p != NULL)
    {
        CUnit* u = p->m_pUnit;
        for (CUnit* end = u + p->m_iSize; u != end; ++u)
            if (u->m_iFlag != 0)
                ++real_count;

        if (p == m_pLastQueue)
            p = NULL;
        else
            p = p->m_pNext;
    }
    m_iCount = real_count;
    if (double(m_iCount) / m_iSize < 0.9)
        return -1;

    CQEntry* tempq = NULL;
    CUnit* tempu = NULL;
    char* tempb = NULL;

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

    m_pLastQueue->m_pNext = tempq;
    m_pLastQueue = tempq;
    m_pLastQueue->m_pNext = m_pQEntry;

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
        return NULL;

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

        m_pCurrQueue = m_pCurrQueue->m_pNext;
        m_pAvailUnit = m_pCurrQueue->m_pUnit;
    } while (m_pCurrQueue != entrance);

    increase();

    return NULL;
}


CSndUList::CSndUList():
    m_pHeap(NULL),
    m_iArrayLength(4096),
    m_iLastEntry(-1),
    m_pTimer(NULL)
{
    m_pHeap = new CSNode*[m_iArrayLength];
}

CSndUList::~CSndUList()
{
    delete[] m_pHeap;
}

void CSndUList::insert(int64_t ts, const CUDT* u)
{
    std::scoped_lock<std::mutex> listguard(m_ListLock);

    // increase the heap array size if necessary
    if (m_iLastEntry == m_iArrayLength - 1)
    {
        CSNode** temp = NULL;

        try
        {
            temp = new CSNode*[m_iArrayLength * 2];
        }
        catch (...)
        {
            return;
        }

        memcpy(temp, m_pHeap, sizeof(CSNode*) * m_iArrayLength);
        m_iArrayLength *= 2;
        delete[] m_pHeap;
        m_pHeap = temp;
    }

    insert_(ts, u);
}

void CSndUList::update(const CUDT* u, bool reschedule)
{
    std::scoped_lock<std::mutex> listguard(m_ListLock);

    CSNode* n = u->m_pSNode;

    if (n->m_iHeapLoc >= 0)
    {
        if (!reschedule)
            return;

        if (n->m_iHeapLoc == 0)
        {
            n->m_llTimeStamp = 1;
            m_pTimer->interrupt();
            return;
        }

        remove_(u);
    }

    insert_(1, u);
}

int CSndUList::pop(sockaddr*& addr, CPacket& pkt)
{
    std::scoped_lock<std::mutex> listguard(m_ListLock);

    if (-1 == m_iLastEntry)
        return -1;

    // no pop until the next schedulled time
    uint64_t ts;
    CTimer::rdtsc(ts);
    if (ts < m_pHeap[0]->m_llTimeStamp)
        return -1;

    CUDT* u = m_pHeap[0]->m_pUDT;
    remove_(u);

    if (!u->m_bConnected || u->m_bBroken)
        return -1;

    // pack a packet from the socket
    if (u->packData(pkt, ts) <= 0)
        return -1;

    addr = u->m_pPeerAddr;

    // insert a new entry, ts is the next processing time
    if (ts > 0)
        insert_(ts, u);

    return 1;
}

void CSndUList::remove(const CUDT* u)
{
    std::scoped_lock<std::mutex> listguard(m_ListLock);

    remove_(u);
}

uint64_t CSndUList::getNextProcTime()
{
    std::scoped_lock<std::mutex> listguard(m_ListLock);

    if (-1 == m_iLastEntry)
        return 0;

    return m_pHeap[0]->m_llTimeStamp;
}

void CSndUList::insert_(int64_t ts, const CUDT* u)
{
    CSNode* n = u->m_pSNode;

    // do not insert repeated node
    if (n->m_iHeapLoc >= 0)
        return;

    m_iLastEntry++;
    m_pHeap[m_iLastEntry] = n;
    n->m_llTimeStamp = ts;

    int q = m_iLastEntry;
    int p = q;
    while (p != 0)
    {
        p = (q - 1) >> 1;
        if (m_pHeap[p]->m_llTimeStamp > m_pHeap[q]->m_llTimeStamp)
        {
            CSNode* t = m_pHeap[p];
            m_pHeap[p] = m_pHeap[q];
            m_pHeap[q] = t;
            t->m_iHeapLoc = q;
            q = p;
        }
        else
            break;
    }

    n->m_iHeapLoc = q;

    // an earlier event has been inserted, wake up sending worker
    if (n->m_iHeapLoc == 0)
        m_pTimer->interrupt();

    // first entry, activate the sending queue
    if (0 == m_iLastEntry)
    {
        std::scoped_lock<std::mutex> lock(*m_pWindowLock);
        m_pWindowCond->notify_all();
    }
}

void CSndUList::remove_(const CUDT* u)
{
    CSNode* n = u->m_pSNode;

    if (n->m_iHeapLoc >= 0)
    {
        // remove the node from heap
        m_pHeap[n->m_iHeapLoc] = m_pHeap[m_iLastEntry];
        m_iLastEntry--;
        m_pHeap[n->m_iHeapLoc]->m_iHeapLoc = n->m_iHeapLoc;

        int q = n->m_iHeapLoc;
        int p = q * 2 + 1;
        while (p <= m_iLastEntry)
        {
            if ((p + 1 <= m_iLastEntry) && (m_pHeap[p]->m_llTimeStamp > m_pHeap[p + 1]->m_llTimeStamp))
                p++;

            if (m_pHeap[q]->m_llTimeStamp > m_pHeap[p]->m_llTimeStamp)
            {
                CSNode* t = m_pHeap[p];
                m_pHeap[p] = m_pHeap[q];
                m_pHeap[p]->m_iHeapLoc = p;
                m_pHeap[q] = t;
                m_pHeap[q]->m_iHeapLoc = q;

                q = p;
                p = q * 2 + 1;
            }
            else
                break;
        }

        m_pHeap[m_iLastEntry + 1] = 0;
        n->m_iHeapLoc = -1;
    }

    // the only event has been deleted, wake up immediately
    if (0 == m_iLastEntry)
        m_pTimer->interrupt();
}

//
CSndQueue::CSndQueue():
    m_pSndUList(NULL),
    m_pChannel(NULL),
    m_pTimer(NULL),
    m_bClosing(false)
{
}

CSndQueue::~CSndQueue()
{
    m_bClosing = true;

    {
        std::scoped_lock<std::mutex> lock(m_WindowLock);
        m_WindowCond.notify_all();
    }

    if (m_WorkerThread.joinable())
        m_WorkerThread.join();

    delete m_pSndUList;
}

void CSndQueue::init(CChannel* c, CTimer* t)
{
    m_pChannel = c;
    m_pTimer = t;
    m_pSndUList = new CSndUList;
    m_pSndUList->m_pWindowLock = &m_WindowLock;
    m_pSndUList->m_pWindowCond = &m_WindowCond;
    m_pSndUList->m_pTimer = m_pTimer;

    // TODO: #ak std::thread can throw. Convert exception to CUDTException(3, 1)?
    m_WorkerThread = std::thread(std::bind(&CSndQueue::worker, this));
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
                [this]() { return m_bClosing || m_pSndUList->m_iLastEntry >= 0; });
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


//
CRcvUList::CRcvUList():
    m_pUList(NULL),
    m_pLast(NULL)
{
}

CRcvUList::~CRcvUList()
{
}

void CRcvUList::insert(const CUDT* u)
{
    CRNode* n = u->m_pRNode;
    CTimer::rdtsc(n->m_llTimeStamp);

    if (NULL == m_pUList)
    {
        // empty list, insert as the single node
        n->m_pPrev = n->m_pNext = NULL;
        m_pLast = m_pUList = n;

        return;
    }

    // always insert at the end for RcvUList
    n->m_pPrev = m_pLast;
    n->m_pNext = NULL;
    m_pLast->m_pNext = n;
    m_pLast = n;
}

void CRcvUList::remove(const CUDT* u)
{
    CRNode* n = u->m_pRNode;

    if (!n->m_bOnList)
        return;

    if (NULL == n->m_pPrev)
    {
        // n is the first node
        m_pUList = n->m_pNext;
        if (NULL == m_pUList)
            m_pLast = NULL;
        else
            m_pUList->m_pPrev = NULL;
    }
    else
    {
        n->m_pPrev->m_pNext = n->m_pNext;
        if (NULL == n->m_pNext)
        {
            // n is the last node
            m_pLast = n->m_pPrev;
        }
        else
            n->m_pNext->m_pPrev = n->m_pPrev;
    }

    n->m_pNext = n->m_pPrev = NULL;
}

void CRcvUList::update(const CUDT* u)
{
    CRNode* n = u->m_pRNode;

    if (!n->m_bOnList)
        return;

    CTimer::rdtsc(n->m_llTimeStamp);

    // if n is the last node, do not need to change
    if (NULL == n->m_pNext)
        return;

    if (NULL == n->m_pPrev)
    {
        m_pUList = n->m_pNext;
        m_pUList->m_pPrev = NULL;
    }
    else
    {
        n->m_pPrev->m_pNext = n->m_pNext;
        n->m_pNext->m_pPrev = n->m_pPrev;
    }

    n->m_pPrev = m_pLast;
    n->m_pNext = NULL;
    m_pLast->m_pNext = n;
    m_pLast = n;
}

//
CHash::CHash():
    m_pBucket(NULL),
    m_iHashSize(0)
{
}

CHash::~CHash()
{
    for (int i = 0; i < m_iHashSize; ++i)
    {
        CBucket* b = m_pBucket[i];
        while (NULL != b)
        {
            CBucket* n = b->m_pNext;
            delete b;
            b = n;
        }
    }

    delete[] m_pBucket;
}

void CHash::init(int size)
{
    m_pBucket = new CBucket*[size];

    for (int i = 0; i < size; ++i)
        m_pBucket[i] = NULL;

    m_iHashSize = size;
}

CUDT* CHash::lookup(int32_t id)
{
    // simple hash function (% hash table size); suitable for socket descriptors
    CBucket* b = m_pBucket[id % m_iHashSize];

    while (NULL != b)
    {
        if (id == b->m_iID)
            return b->m_pUDT;
        b = b->m_pNext;
    }

    return NULL;
}

void CHash::insert(int32_t id, CUDT* u)
{
    CBucket* b = m_pBucket[id % m_iHashSize];

    CBucket* n = new CBucket;
    n->m_iID = id;
    n->m_pUDT = u;
    n->m_pNext = b;

    m_pBucket[id % m_iHashSize] = n;
}

void CHash::remove(int32_t id)
{
    CBucket* b = m_pBucket[id % m_iHashSize];
    CBucket* p = NULL;

    while (NULL != b)
    {
        if (id == b->m_iID)
        {
            if (NULL == p)
                m_pBucket[id % m_iHashSize] = b->m_pNext;
            else
                p->m_pNext = b->m_pNext;

            delete b;

            return;
        }

        p = b;
        b = b->m_pNext;
    }
}


//
CRendezvousQueue::CRendezvousQueue():
    m_lRendezvousID(),
    m_RIDVectorLock()
{
#ifndef _WIN32
    pthread_mutex_init(&m_RIDVectorLock, NULL);
#else
    m_RIDVectorLock = CreateMutex(NULL, false, NULL);
#endif
}

CRendezvousQueue::~CRendezvousQueue()
{
#ifndef _WIN32
    pthread_mutex_destroy(&m_RIDVectorLock);
#else
    CloseHandle(m_RIDVectorLock);
#endif

    for (list<CRL>::iterator i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++i)
    {
        if (AF_INET == i->m_iIPversion)
            delete (sockaddr_in*)i->m_pPeerAddr;
        else
            delete (sockaddr_in6*)i->m_pPeerAddr;
    }

    m_lRendezvousID.clear();
}

void CRendezvousQueue::insertToRQ(const UDTSOCKET& id, CUDT* u, int ipv, const sockaddr* addr, uint64_t ttl)
{
    CGuard vg(m_RIDVectorLock);

    CRL r;
    r.m_iID = id;
    r.m_pUDT = u;
    r.m_iIPversion = ipv;
    r.m_pPeerAddr = (AF_INET == ipv) ? (sockaddr*)new sockaddr_in : (sockaddr*)new sockaddr_in6;
    memcpy(r.m_pPeerAddr, addr, (AF_INET == ipv) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));
    r.m_ullTTL = ttl;

    m_lRendezvousID.push_back(r);
}

void CRendezvousQueue::removeFromRQ(const UDTSOCKET& id)
{
    CGuard vg(m_RIDVectorLock);

    for (list<CRL>::iterator i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++i)
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

CUDT* CRendezvousQueue::retrieveFromRQ(const sockaddr* addr, UDTSOCKET& id)
{
    CGuard vg(m_RIDVectorLock);

    // TODO: optimize search
    for (list<CRL>::iterator i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++i)
    {
        if (CIPAddress::ipcmp(addr, i->m_pPeerAddr, i->m_iIPversion) && ((0 == id) || (id == i->m_iID)))
        {
            id = i->m_iID;
            return i->m_pUDT;
        }
    }

    return NULL;
}

void CRendezvousQueue::updateConnStatus()
{
    if (m_lRendezvousID.empty())
        return;

    CGuard vg(m_RIDVectorLock);

    for (list<CRL>::iterator i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++i)
    {
        // avoid sending too many requests, at most 1 request per 250ms
        if (CTimer::getTime() - i->m_pUDT->m_llLastReqTime > 250000)
        {
            if (CTimer::getTime() >= i->m_ullTTL)
            {
                // connection timer expired, acknowledge app via epoll
                i->m_pUDT->setConnecting(false);
                CUDT::s_UDTUnited.m_EPoll.update_events(i->m_iID, i->m_pUDT->m_sPollID, UDT_EPOLL_ERR, true);
                continue;
            }

            CPacket request;
            char* reqdata = new char[i->m_pUDT->m_iPayloadSize];
            request.pack(ControlPacketType::Handshake, NULL, reqdata, i->m_pUDT->m_iPayloadSize);
            // ID = 0, connection request
            request.m_iID = !i->m_pUDT->m_bRendezvous ? 0 : i->m_pUDT->m_ConnRes.m_iID;
            int hs_size = i->m_pUDT->m_iPayloadSize;
            i->m_pUDT->m_ConnReq.serialize(reqdata, hs_size);
            request.setLength(hs_size);
            i->m_pUDT->m_pSndQueue->sendto(i->m_pPeerAddr, request);
            i->m_pUDT->m_llLastReqTime = CTimer::getTime();
            delete[] reqdata;
        }
    }
}

//
CRcvQueue::CRcvQueue():
    m_UnitQueue(),
    m_pRcvUList(NULL),
    m_pHash(NULL),
    m_pChannel(NULL),
    m_pTimer(NULL),
    m_iPayloadSize(),
    m_bClosing(false),
    m_pListener(NULL),
    m_pRendezvousQueue(NULL),
    m_vNewEntry(),
    m_mBuffer()
{
}

CRcvQueue::~CRcvQueue()
{
    m_bClosing = true;

    if (m_WorkerThread.joinable())
        m_WorkerThread.join();

    delete m_pRcvUList;
    delete m_pHash;
    delete m_pRendezvousQueue;

    // remove all queued messages
    for (map<int32_t, std::queue<CPacket*> >::iterator i = m_mBuffer.begin(); i != m_mBuffer.end(); ++i)
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

void CRcvQueue::init(int qsize, int payload, int version, int hsize, CChannel* cc, CTimer* t)
{
    m_iPayloadSize = payload;

    m_UnitQueue.init(qsize, payload, version);

    m_pHash = new CHash;
    m_pHash->init(hsize);

    m_pChannel = cc;
    m_pTimer = t;

    m_pRcvUList = new CRcvUList;
    m_pRendezvousQueue = new CRendezvousQueue;

    m_WorkerThread = std::thread(&CRcvQueue::worker, this);
    // TODO: #ak Convert std::thread exception to CUDTException(3, 1)?
}

void CRcvQueue::worker()
{
    sockaddr* addr = (AF_INET == m_UnitQueue.m_iIPversion) ? (sockaddr*) new sockaddr_in : (sockaddr*) new sockaddr_in6;
    CUDT* u = NULL;
    int32_t id;

    while (!m_bClosing)
    {
#ifdef NO_BUSY_WAITING
        m_pTimer->tick();
#endif

        // check waiting list, if new socket, insert it to the list
        while (ifNewEntry())
        {
            CUDT* ne = getNewEntry();
            if (NULL != ne)
            {
                m_pRcvUList->insert(ne);
                m_pHash->insert(ne->m_SocketID, ne);
            }
        }

        // find next available slot for incoming packet
        CUnit* unit = m_UnitQueue.getNextAvailUnit();
        if (NULL == unit)
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
                if (NULL != m_pListener)
                    m_pListener->listen(addr, unit->m_Packet);
                else if (NULL != (u = m_pRendezvousQueue->retrieveFromRQ(addr, id)))
                {
                    // asynchronous connect: call connect here
                    // otherwise wait for the UDT socket to retrieve this packet
                    if (!u->m_bSynRecving)
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

                if (NULL != (u = m_pHash->lookup(id)))
                {
                    if (CIPAddress::ipcmp(addr, u->m_pPeerAddr, u->m_iIPversion))
                    {
                        if (u->m_bConnected && !u->m_bBroken && !u->isClosing())
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
                else if (NULL != (u = m_pRendezvousQueue->retrieveFromRQ(addr, id)))
                {
                    if (!u->m_bSynRecving)
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
        while ((NULL != ul) && (ul->m_llTimeStamp < ctime))
        {
            CUDT* u = ul->m_pUDT;

            if (u->m_bConnected && !u->m_bBroken && !u->isClosing())
            {
                u->checkTimers(false);
                m_pRcvUList->update(u);
            }
            else
            {
                // the socket must be removed from Hash table first, then RcvUList
                m_pHash->remove(u->m_SocketID);
                m_pRcvUList->remove(u);
                u->m_pRNode->m_bOnList = false;
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

int CRcvQueue::setListener(CUDT* u)
{
    std::scoped_lock<std::mutex> lock(m_LSLock);

    if (NULL != m_pListener)
        return -1;

    m_pListener = u;
    return 0;
}

void CRcvQueue::removeListener(const CUDT* u)
{
    std::scoped_lock<std::mutex> lock(m_LSLock);

    if (u == m_pListener)
        m_pListener = NULL;
}

void CRcvQueue::registerConnector(const UDTSOCKET& id, CUDT* u, int ipv, const sockaddr* addr, uint64_t ttl)
{
    m_pRendezvousQueue->insertToRQ(id, u, ipv, addr, ttl);
}

void CRcvQueue::removeConnector(const UDTSOCKET& id)
{
    m_pRendezvousQueue->removeFromRQ(id);

    std::scoped_lock<std::mutex> bufferlock(m_PassLock);

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

void CRcvQueue::setNewEntry(CUDT* u)
{
    std::scoped_lock<std::mutex> lock(m_IDLock);
    m_vNewEntry.push_back(u);
}

bool CRcvQueue::ifNewEntry()
{
    return !(m_vNewEntry.empty());
}

CUDT* CRcvQueue::getNewEntry()
{
    std::scoped_lock<std::mutex> lock(m_IDLock);

    if (m_vNewEntry.empty())
        return NULL;

    CUDT* u = (CUDT*)*(m_vNewEntry.begin());
    m_vNewEntry.erase(m_vNewEntry.begin());

    return u;
}

void CRcvQueue::storePkt(int32_t id, CPacket* pkt)
{
    std::scoped_lock<std::mutex> bufferlock(m_PassLock);

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
