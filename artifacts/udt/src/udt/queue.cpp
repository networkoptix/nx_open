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

#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <functional>

#include "common.h"
#include "core.h"
#include "queue.h"

using namespace std;

//#define DEBUG_RECORD_PACKET_HISTORY
//#define TRACE_PACKETS

namespace {

#if defined(TRACE_PACKETS) || defined(DEBUG_RECORD_PACKET_HISTORY)
static std::string dumpPacket(const CPacket& packet)
{
    std::ostringstream ss;
    ss << "[" << packet.m_iID << ", " << (int)packet.getType() <<"]. ";
    if (packet.getFlag() == PacketFlag::Data)
       ss << std::string(packet.payload().data(), packet.payload().size());
    else
       ss << "type: "<<to_string(packet.getType())<<" [other flags]";
    return ss.str();
}
#endif

#if defined(TRACE_PACKETS)

static std::mutex tracePacketMutex;

void tracePacket(
    const std::string& direction,
    const detail::SocketAddress& from,
    const detail::SocketAddress& to,
    const CPacket& packet)
{
    std::lock_guard<std::mutex> lock(tracePacketMutex);

    std::cout << direction << ". " <<
        from.toString() << "->" << to.toString() << ". " <<
        dumpPacket(packet) << std::endl;
}

#endif // TRACE_PACKETS

#if defined(DEBUG_RECORD_PACKET_HISTORY)

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

#endif // DEBUG_RECORD_PACKET_HISTORY

} // namespace

//-------------------------------------------------------------------------------------------------

CSndUList::CSndUList(
    CTimer* timer,
    std::mutex* windowLock,
    std::condition_variable* windowCond)
    :
    m_iArrayLength(4096),
    m_pWindowLock(windowLock),
    m_pWindowCond(windowCond),
    m_timer(timer)
{
    m_nodeHeap.resize(m_iArrayLength, nullptr);
}

CSndUList::~CSndUList()
{
}

void CSndUList::update(std::shared_ptr<CUDT> u, bool reschedule)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto [it, inserted] = m_socketToNode.emplace(u.get(), nullptr);
    if (inserted)
    {
        it->second = std::make_unique<CSNode>();
        it->second->socket = u;
        it->second->timestamp = std::chrono::microseconds(1);
    }

    auto n = it->second.get();

    if (n->locationOnHeap >= 0)
    {
        if (!reschedule)
            return;

        if (n->locationOnHeap == 0)
        {
            n->timestamp = std::chrono::microseconds(1);
            m_timer->interrupt();
            return;
        }

        remove_(n);
    }

    insert_(std::chrono::microseconds(1), n);
}

int CSndUList::pop(detail::SocketAddress& addr, CPacket& pkt)
{
    // When this method destroyes CUDT, it must do it with no mutex lock.
    std::shared_ptr<CUDT> u;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (-1 == m_iLastEntry)
        return -1;

    auto n = m_nodeHeap[0];

    // no pop until the next scheduled time
    auto ts = CTimer::getTime();
    if (ts < n->timestamp)
        return -1;

    u = n->socket.lock();
    remove_(n);

    if (!u || !u->connected() || u->broken())
        return -1;

    // pack a packet from the socket
    if (u->packData(pkt, ts) <= 0)
        return -1;

    addr = u->peerAddr();

    // insert a new entry, ts is the next processing time
    if (ts > std::chrono::microseconds::zero())
        insert_(ts, n);

    return 1;
}

void CSndUList::remove(CUDT* u)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_socketToNode.find(u);
    if (it == m_socketToNode.end())
        return;

    auto n = std::exchange(it->second, nullptr);
    m_socketToNode.erase(it);

    remove_(n.get());
}

std::chrono::microseconds CSndUList::getNextProcTime() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (-1 == m_iLastEntry)
        return std::chrono::microseconds::zero();

    return m_nodeHeap[0]->timestamp;
}

void CSndUList::insert_(
    std::chrono::microseconds ts,
    CSNode* n)
{
    // do not insert repeated node
    if (n->locationOnHeap >= 0)
        return;

    m_iLastEntry++;
    m_nodeHeap[m_iLastEntry] = n;
    n->timestamp = ts;

    int q = m_iLastEntry;
    int p = q;
    while (p != 0)
    {
        p = (q - 1) >> 1;
        if (m_nodeHeap[p]->timestamp > m_nodeHeap[q]->timestamp)
        {
            std::swap(m_nodeHeap[p], m_nodeHeap[q]);
            m_nodeHeap[q]->locationOnHeap = q;
            q = p;
        }
        else
        {
            break;
        }
    }

    n->locationOnHeap = q;

    // an earlier event has been inserted, wake up sending worker
    if (n->locationOnHeap == 0)
        m_timer->interrupt();

    // first entry, activate the sending queue
    if (0 == m_iLastEntry)
    {
        std::lock_guard<std::mutex> lock(*m_pWindowLock);
        m_pWindowCond->notify_all();
    }
}

void CSndUList::remove_(CSNode* n)
{
    if (n->locationOnHeap >= 0)
    {
        // remove the node from heap
        m_nodeHeap[n->locationOnHeap] = m_nodeHeap[m_iLastEntry];
        m_iLastEntry--;
        m_nodeHeap[n->locationOnHeap]->locationOnHeap = n->locationOnHeap;

        int q = n->locationOnHeap;
        int p = q * 2 + 1;
        while (p <= m_iLastEntry)
        {
            if ((p + 1 <= m_iLastEntry) && (m_nodeHeap[p]->timestamp > m_nodeHeap[p + 1]->timestamp))
                p++;

            if (m_nodeHeap[q]->timestamp > m_nodeHeap[p]->timestamp)
            {
                auto t = m_nodeHeap[p];
                m_nodeHeap[p] = m_nodeHeap[q];
                m_nodeHeap[p]->locationOnHeap = p;
                m_nodeHeap[q] = t;
                m_nodeHeap[q]->locationOnHeap = q;

                q = p;
                p = q * 2 + 1;
            }
            else
                break;
        }

        m_nodeHeap[m_iLastEntry + 1] = 0;
        n->locationOnHeap = -1;
    }

    // the only event has been deleted, wake up immediately
    if (0 == m_iLastEntry)
        m_timer->interrupt();
}

//
CSndQueue::CSndQueue(AbstractUdpChannel* c, CTimer* t):
    m_pSndUList(std::make_unique<CSndUList>(t, &m_mutex, &m_cond)),
    m_channel(c)
{
}

CSndQueue::~CSndQueue()
{
    m_bClosing = true;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cond.notify_all();
    }

    if (m_WorkerThread.joinable())
        m_WorkerThread.join();

    m_pSndUList.reset();
}

Result<> CSndQueue::start()
{
    try
    {
        m_WorkerThread = std::thread([this]() { worker(); });
    }
    catch (const std::system_error& e)
    {
        return Error(e.code().value());
    }

    return success();
}

void CSndQueue::worker()
{
    setCurrentThreadName(typeid(*this).name());

    while (!m_bClosing)
    {
        sendPostedPackets();

        const auto ts = m_pSndUList->getNextProcTime();
        if (ts > std::chrono::microseconds::zero())
        {
            // wait until next processing time of the first socket on the list
            const auto currtime = CTimer::getTime();
            if (currtime < ts)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_cond.wait_for(
                        lock,
                        ts - currtime,
                        [this]() { return m_bClosing || !m_sendTasks.empty(); }))
                {
                    // m_sendTasks is not empty.
                    continue;
                }
            }

            // it is time to send the next pkt
            detail::SocketAddress addr;
            CPacket pkt;
            if (m_pSndUList->pop(addr, pkt) < 0)
                continue;

#ifdef DEBUG_RECORD_PACKET_HISTORY
            packetVerifier.beforeSendingPacket(pkt);
#endif // DEBUG_RECORD_PACKET_HISTORY

            postPacket(addr, std::move(pkt));
        }
        else
        {
            // wait here if there is no sockets with data to be sent
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cond.wait(
                lock,
                [this]() { return m_bClosing || m_pSndUList->lastEntry() >= 0 || !m_sendTasks.empty(); });
        }
    }
}

int CSndQueue::sendto(const detail::SocketAddress& addr, CPacket packet)
{
    const auto packetLength = packet.getLength();
    postPacket(addr, std::move(packet));
    return packetLength;
}

detail::SocketAddress CSndQueue::getLocalAddr() const
{
    return m_channel->getSockAddr();
}

int CSndQueue::sendPacket(const detail::SocketAddress& addr, CPacket packet)
{
#ifdef DEBUG_RECORD_PACKET_HISTORY
    packetVerifier.beforeSendingPacket(packet);
#endif // DEBUG_RECORD_PACKET_HISTORY

    // send out the packet immediately (high priority), this is a control packet
    const auto packetLength = packet.getLength();

#ifdef TRACE_PACKETS
    tracePacket("send", m_channel->getSockAddr(), addr, packet);
#endif

    m_channel->sendto(addr, std::move(packet));
    return packetLength;
}

void CSndQueue::postPacket(const detail::SocketAddress& addr, CPacket packet)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sendTasks.push_back({addr, std::move(packet)});
    }

    m_cond.notify_all();
}

void CSndQueue::sendPostedPackets()
{
    decltype(m_sendTasks) sendTasks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sendTasks = std::exchange(m_sendTasks, {});
    }

    for (auto& task: sendTasks)
        sendPacket(task.addr, std::move(task.packet));
}

//-------------------------------------------------------------------------------------------------
// CRcvUList

void CRcvUList::push_back(std::shared_ptr<CUDT> u)
{
    const auto id = u->socketId();

    if (auto it = m_socketIdToNode.find(id); it != m_socketIdToNode.end())
        m_sockets.erase(it->second);

    m_sockets.push_back(CRNode{.socketId = id, .socket = u, .timestamp = CTimer::getTime()});
    m_socketIdToNode[id] = std::prev(m_sockets.end());
}

void CRcvUList::erase(std::list<CRNode>::iterator it)
{
    m_socketIdToNode.erase(it->socketId);
    m_sockets.erase(it);
}

void CRcvUList::sink(UDTSOCKET socketId)
{
    auto it = m_socketIdToNode.find(socketId);
    if (it == m_socketIdToNode.end())
        return;

    it->second->timestamp = CTimer::getTime();
    m_sockets.splice(m_sockets.end(), m_sockets, it->second);
    it->second = std::prev(m_sockets.end());
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

CRendezvousQueue::CRL::CRL()
{
}

CRendezvousQueue::CRendezvousQueue()
{
}

CRendezvousQueue::~CRendezvousQueue()
{
    m_rendezvousSockets.clear();
}

void CRendezvousQueue::insert(
    const UDTSOCKET& id,
    std::weak_ptr<CUDT> u,
    const detail::SocketAddress& addr,
    std::chrono::microseconds ttl)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    CRL r;
    r.id = id;
    r.socket = u;
    r.peerAddr = addr;
    r.ttl = ttl;

    m_rendezvousSockets.push_back(r);
}

void CRendezvousQueue::remove(const UDTSOCKET& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto i = m_rendezvousSockets.begin(); i != m_rendezvousSockets.end(); ++i)
    {
        if (i->id == id)
        {
            m_rendezvousSockets.erase(i);
            return;
        }
    }
}

std::shared_ptr<CUDT> CRendezvousQueue::getByAddr(
    const detail::SocketAddress& addr,
    UDTSOCKET& id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // TODO: optimize search
    for (const auto& crl: m_rendezvousSockets)
    {
        if (addr == crl.peerAddr && (id == 0 || id == crl.id))
        {
            id = crl.id;
            return crl.socket.lock();
        }
    }

    return nullptr;
}

void CRendezvousQueue::updateConnStatus()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_rendezvousSockets.empty())
        return;

    for (auto i = m_rendezvousSockets.begin(); i != m_rendezvousSockets.end(); ++i)
    {
        auto udtSocket = i->socket.lock();
        if (!udtSocket)
            continue;

        // avoid sending too many requests, at most 1 request per 250ms
        if (CTimer::getTime() - udtSocket->lastReqTime() > kSyncRepeatMinPeriod)
        {
            if (CTimer::getTime() >= i->ttl)
            {
                // connection timer expired, acknowledge app via epoll
                udtSocket->setConnecting(false);
                CUDT::s_UDTUnited->m_EPoll.update_events(
                    i->id,
                    udtSocket->pollIds(),
                    UDT_EPOLL_ERR,
                    true);
                continue;
            }

            CPacket request;
            request.pack(ControlPacketType::Handshake, nullptr, udtSocket->payloadSize());
            // ID = 0, connection request
            request.m_iID = !udtSocket->rendezvous() ? 0 : udtSocket->connRes().m_iID;
            int hs_size = udtSocket->payloadSize();
            udtSocket->connReq().serialize(request.payload().data(), hs_size);
            request.setLength(hs_size);
            udtSocket->sndQueue().sendto(i->peerAddr, std::move(request));
            udtSocket->setLastReqTime(CTimer::getTime());
        }
    }
}

//-------------------------------------------------------------------------------------------------
// CRcvQueue

CRcvQueue::CRcvQueue(
    int size,
    int payload,
    int ipVersion,
    AbstractUdpChannel* c,
    CTimer* t)
    :
    m_UnitQueue(size, payload),
    m_channel(c),
    m_timer(t),
    m_iIPversion(ipVersion),
    m_iPayloadSize(payload),
    m_bClosing(false),
    m_pRendezvousQueue(std::make_unique<CRendezvousQueue>())
{
    assert(m_iPayloadSize > 0);
}

CRcvQueue::~CRcvQueue()
{
    stop();
}

Result<> CRcvQueue::start()
{
    try
    {
        m_WorkerThread = std::thread(&CRcvQueue::worker, this);
    }
    catch (const std::system_error& e)
    {
        return Error(e.code().value());
    }

    return success();
}

void CRcvQueue::stop()
{
    m_bClosing = true;

    if (m_WorkerThread.joinable())
        m_WorkerThread.join();
}

void CRcvQueue::worker()
{
    setCurrentThreadName(typeid(*this).name());

    detail::SocketAddress addr(m_iIPversion);

    while (!m_bClosing)
    {
#ifdef NO_BUSY_WAITING
        m_timer->tick();
#endif

        // check waiting list, if new socket, insert it to the list
        while (std::shared_ptr<CUDT> ne = takeNewEntry())
        {
            m_rcvUList.push_back(ne);
            m_socketByIdDict.insert(ne->socketId(), ne);
        }

        // find next available slot for incoming packet
        auto unit = m_UnitQueue.takeNextAvailUnit();
        unit.packet().payload().resize(m_iPayloadSize);

        // Reading next incoming packet, recvfrom returns -1 if nothing has been received.
        if (!m_channel->recvfrom(addr, unit.packet()).ok())
        {
            timerCheck();
            continue;
        }

#ifdef TRACE_PACKETS
        tracePacket("recv", addr, m_channel->getSockAddr(), unit->packet());
#endif

        processUnit(std::move(unit), addr);
        // Ignoring error since the socket could be removed before connect finished.

        timerCheck();
    }
}

// TODO: #akolesnikov Name properly.
static constexpr auto kSomeThreshold = std::chrono::milliseconds(100);

void CRcvQueue::timerCheck()
{
    const auto ctime = CTimer::getTime() - kSomeThreshold;
    for (auto ul = m_rcvUList.begin();
        ((ul != m_rcvUList.end()) && (ul->timestamp < ctime));
        ul = m_rcvUList.begin())
    {
        std::shared_ptr<CUDT> u = ul->socket.lock();

        if (u && u->connected() && !u->broken() && !u->isClosing())
        {
            u->checkTimers(false);
            m_rcvUList.sink(ul->socketId);
        }
        else
        {
            // the socket must be removed from Hash table first, then RcvUList
            m_socketByIdDict.remove(ul->socketId);
            m_rcvUList.erase(ul);
        }
    }

    // Check connection requests status for all sockets in the RendezvousQueue.
    m_pRendezvousQueue->updateConnStatus();
}

Result<> CRcvQueue::processUnit(Unit unit, const detail::SocketAddress& addr)
{
    int32_t id = unit.packet().m_iID;

    // ID 0 is for connection request, which should be passed to the listening socket or rendezvous sockets
    if (0 == id)
    {
        if (auto listener = m_listener.lock())
        {
            listener->processConnectionRequest(addr, unit.packet());
        }
        else if (auto u = m_pRendezvousQueue->getByAddr(addr, id))
        {
            // asynchronous connect: call connect here
            // otherwise wait for the UDT socket to retrieve this packet
            if (!u->synRecving())
                u->connect(unit.packet()); // TODO: #akolesnikov The result code is ignored here.
            else
                storePkt(id, unit.packet().clone());
        }
    }
    else if (id > 0)
    {
#ifdef DEBUG_RECORD_PACKET_HISTORY
        packetVerifier.packetReceived(unit->packet());
#endif // DEBUG_RECORD_PACKET_HISTORY

        if (auto u = m_socketByIdDict.lookup(id))
        {
            if (addr == u->peerAddr())
            {
                if (u->connected() && !u->broken() && !u->isClosing())
                {
                    if (unit.packet().getFlag() == PacketFlag::Data)
                        u->processData(std::move(unit)); // TODO: #akolesnikov It is unclear why the result is ignored.
                    else
                        u->processCtrl(unit.packet());

                    u->checkTimers(false);
                    m_rcvUList.sink(id);
                }
            }
        }
        else if (auto u = m_pRendezvousQueue->getByAddr(addr, id))
        {
            if (!u->synRecving())
                u->connect(unit.packet()); // TODO: #akolesnikov The result code is ignored here.
            else
                storePkt(id, unit.packet().clone());
        }
    }

    return success();
}

int CRcvQueue::recvfrom(
    int32_t id,
    CPacket& packet,
    std::chrono::microseconds timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto i = m_packets.find(id);

    if (i == m_packets.end())
    {
        m_cond.wait_for(lock, timeout);

        i = m_packets.find(id);
        if (i == m_packets.end())
        {
            packet.setLength(-1);
            return -1;
        }
    }

    // retrieve the earliest packet
    auto& newpkt = i->second.front();

    if (packet.getLength() < newpkt->getLength())
    {
        packet.setLength(-1);
        return -1;
    }

    // copy packet content
    memcpy(packet.header(), newpkt->header(), kPacketHeaderSize);
    packet.setPayload(newpkt->payload());

    // remove this message from queue,
    // if no more messages left for this socket, release its data structure
    i->second.pop();
    if (i->second.empty())
        m_packets.erase(i);

    return packet.getLength();
}

bool CRcvQueue::setListener(std::weak_ptr<ServerSideConnectionAcceptor> listener)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_listener.lock())
        return false;

    m_listener = listener;
    return true;
}

void CRcvQueue::registerConnector(
    const UDTSOCKET& id,
    std::shared_ptr<CUDT> u,
    const detail::SocketAddress& addr,
    std::chrono::microseconds ttl)
{
    m_pRendezvousQueue->insert(id, u, addr, ttl);
}

void CRcvQueue::removeConnector(const UDTSOCKET& id)
{
    m_pRendezvousQueue->remove(id);

    std::lock_guard<std::mutex> lock(m_mutex);

    m_packets.erase(id);
}

void CRcvQueue::addNewEntry(const std::weak_ptr<CUDT>& u)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vNewEntry.push_back(u);
}

std::shared_ptr<CUDT> CRcvQueue::takeNewEntry()
{
    std::shared_ptr<CUDT> u;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_vNewEntry.empty())
        return nullptr;

    u = m_vNewEntry.front().lock();
    m_vNewEntry.erase(m_vNewEntry.begin());

    return u;
}

void CRcvQueue::storePkt(int32_t id, std::unique_ptr<CPacket> pkt)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto i = m_packets.find(id);

    if (i == m_packets.end())
    {
        m_packets[id].push(std::move(pkt));

        m_cond.notify_all();
    }
    else
    {
        //avoid storing too many packets, in case of malfunction or attack
        if (i->second.size() > 16)
            return;

        i->second.push(std::move(pkt));
    }
}
