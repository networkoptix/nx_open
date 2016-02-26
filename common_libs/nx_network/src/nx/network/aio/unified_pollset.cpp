
#include "unified_pollset.h"

#include <iostream>

#include <udt/udt.h>

#include "../udt/udt_common.h"
#include "../udt/udt_socket.h"
#include "../udt/udt_socket_impl.h"


namespace nx {
namespace network {
namespace aio {

namespace {
int mapAioEventToUdtEvent(aio::EventType et)
{
    switch (et)
    {
        case aio::etRead:
            return UDT_EPOLL_IN | UDT_EPOLL_ERR;
        case aio::etWrite:
            return UDT_EPOLL_OUT | UDT_EPOLL_ERR;
        default:
            Q_ASSERT(0);
            return 0;
    }
}
}

/*********************************
ConstIteratorImpl
**********************************/
class ConstIteratorImpl
{
public:
    UnifiedPollSet* pollSet;
    CurrentSet currentSet;
    std::set<UDTSOCKET>::const_iterator udtSocketIter;
    std::set<AbstractSocket::SOCKET_HANDLE>::const_iterator sysSocketIter;

    ConstIteratorImpl(UnifiedPollSet* _pollSet)
    :
        pollSet(_pollSet),
        currentSet(CurrentSet::none)
    {
        pollSet->m_iterators.insert(this);
    }

    ~ConstIteratorImpl()
    {
        assert(pollSet->m_iterators.erase(this) == 1);
    }
};


/*********************************
UnifiedPollSet::const_iterator
**********************************/
UnifiedPollSet::const_iterator::const_iterator()
:
    m_impl(nullptr)
{
}

UnifiedPollSet::const_iterator::const_iterator(
    UnifiedPollSet::const_iterator&& rhs)
{
    m_impl = rhs.m_impl;
    rhs.m_impl = nullptr;
}

UnifiedPollSet::const_iterator::~const_iterator()
{
    delete m_impl;
    m_impl = nullptr;
}

UnifiedPollSet::const_iterator& UnifiedPollSet::const_iterator::operator=(
    UnifiedPollSet::const_iterator&& rhs)
{
    if (this == &rhs)
        return *this;

    m_impl = rhs.m_impl;
    rhs.m_impl = nullptr;

    return *this;
}

UnifiedPollSet::const_iterator UnifiedPollSet::const_iterator::operator++(int)  //it++
{
    const_iterator newIter;
    newIter.m_impl = new ConstIteratorImpl(m_impl->pollSet);
    newIter.m_impl->currentSet = m_impl->currentSet;
    newIter.m_impl->sysSocketIter = m_impl->sysSocketIter;
    newIter.m_impl->udtSocketIter = m_impl->udtSocketIter;

    m_impl->pollSet->moveIterToTheNextEvent(m_impl);

    return newIter;
}

UnifiedPollSet::const_iterator& UnifiedPollSet::const_iterator::operator++()    //++it
{
    m_impl->pollSet->moveIterToTheNextEvent(m_impl);
    return *this;
}

Pollable* UnifiedPollSet::const_iterator::socket()
{
    switch (m_impl->currentSet)
    {
        case CurrentSet::udtRead:
        case CurrentSet::udtWrite:
            return m_impl->pollSet->m_udtSockets.find(*m_impl->udtSocketIter)->second.socket;
        case CurrentSet::sysRead:
        case CurrentSet::sysWrite:
            return m_impl->pollSet->m_sysSockets.find(*m_impl->sysSocketIter)->second.socket;
        default:
            return nullptr;
    }
}

const Pollable* UnifiedPollSet::const_iterator::socket() const
{
    switch (m_impl->currentSet)
    {
        case CurrentSet::udtRead:
        case CurrentSet::udtWrite:
            return m_impl->pollSet->m_udtSockets.find(*m_impl->udtSocketIter)->second.socket;
        case CurrentSet::sysRead:
        case CurrentSet::sysWrite:
            return m_impl->pollSet->m_sysSockets.find(*m_impl->sysSocketIter)->second.socket;
        default:
            return nullptr;
    }
}

aio::EventType UnifiedPollSet::const_iterator::eventType() const
{
    switch (m_impl->currentSet)
    {
        case CurrentSet::udtRead:
        case CurrentSet::sysRead:
            return aio::etRead;
        case CurrentSet::udtWrite:
        case CurrentSet::sysWrite:
            return aio::etWrite;
        default:
            assert(false);
            return aio::etNone;
    }
}

bool UnifiedPollSet::const_iterator::operator==(
    const UnifiedPollSet::const_iterator& right) const
{
    if (m_impl == nullptr || right.m_impl == nullptr)
        return m_impl == right.m_impl;

    return m_impl->currentSet == right.m_impl->currentSet
        && m_impl->pollSet == right.m_impl->pollSet
        && m_impl->sysSocketIter == right.m_impl->sysSocketIter
        && m_impl->udtSocketIter == right.m_impl->udtSocketIter;
}

bool UnifiedPollSet::const_iterator::operator!=(
    const UnifiedPollSet::const_iterator& right) const
{
    return !(*this == right);
}


/*********************************
UnifiedPollSet
**********************************/

UnifiedPollSet::UnifiedPollSet()
:
    m_epollFd(-1)
{
    m_epollFd = UDT::epoll_create();
    m_interruptSocket.setNonBlockingMode(true);
    m_interruptSocket.bind(SocketAddress(HostAddress::localhost, 0));
    if (!add(&m_interruptSocket, aio::etRead))
        m_interruptSocket.close();
}

UnifiedPollSet::~UnifiedPollSet()
{
    UDT::epoll_release(m_epollFd);
}

bool UnifiedPollSet::isValid() const
{
    return (m_epollFd != -1) && (m_interruptSocket.handle() != INVALID_SOCKET);
}

void UnifiedPollSet::interrupt()
{
    quint8 buf[128];
    m_interruptSocket.sendTo(buf, sizeof(buf), m_interruptSocket.getLocalAddress());
}

bool UnifiedPollSet::add(Pollable* const sock, EventType eventType, void* /*userData*/)
{
    int udtEvent = mapAioEventToUdtEvent(eventType);

    if (sock->impl()->isUdtSocket)
        return addUdtSocket(
            static_cast<UDTSocketImpl*>(sock->impl())->udtHandle,
            udtEvent,
            sock);
    else
        return addSysSocket(
            sock->handle(),
            udtEvent,
            sock);
}

void UnifiedPollSet::remove(Pollable* const sock, EventType eventType)
{
    int udtEvent = mapAioEventToUdtEvent(eventType);
    if (sock->impl()->isUdtSocket)
    {
        auto udtHandle = static_cast<UDTSocketImpl*>(sock->impl())->udtHandle;
        removeUdtSocket(udtHandle, udtEvent);

        if ((udtEvent & UDT_EPOLL_IN) > 0 &&
            !isElementBeingUsed(CurrentSet::udtRead, udtHandle))
        {
            m_readUdtFds.erase(udtHandle);
        }
        if ((udtEvent & UDT_EPOLL_OUT) > 0 &&
            !isElementBeingUsed(CurrentSet::udtWrite, udtHandle))
        {
            m_writeUdtFds.erase(udtHandle);
        }
    }
    else
    {
        removeSysSocket(sock->handle(), udtEvent);

        if ((udtEvent & UDT_EPOLL_IN) > 0 &&
            !isElementBeingUsed(CurrentSet::sysRead, sock->handle()))
        {
            m_readSysFds.erase(sock->handle());
        }
        if ((udtEvent & UDT_EPOLL_OUT) > 0 &&
            !isElementBeingUsed(CurrentSet::sysWrite, sock->handle()))
        {
            m_writeSysFds.erase(sock->handle());
        }
    }
}

size_t UnifiedPollSet::size() const
{
    return m_udtSockets.size() + m_sysSockets.size();
}

int UnifiedPollSet::poll(int millisToWait)
{
    m_readUdtFds.clear();
    m_writeUdtFds.clear();
    m_readSysFds.clear();
    m_writeSysFds.clear();

    int result = UDT::epoll_wait(
        m_epollFd,
        &m_readUdtFds, &m_writeUdtFds,
        millisToWait,
        &m_readSysFds, &m_writeSysFds);
    if (result < 0)
    {
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return -1;
    }

    auto it = m_readSysFds.find(m_interruptSocket.handle());
    if (it != m_readSysFds.end())
    {
        --result;
        quint8 buf[128];
        m_interruptSocket.recv(buf, sizeof(buf), 0);   //ignoring result and data...
        m_readSysFds.erase(it);
    }
    return result;
}

UnifiedPollSet::const_iterator UnifiedPollSet::begin() const
{
    const_iterator iter;
    iter.m_impl = new ConstIteratorImpl(const_cast<UnifiedPollSet*>(this));
    iter.m_impl->currentSet = CurrentSet::none;
    moveIterToTheNextEvent(iter.m_impl);
    return iter;
}

UnifiedPollSet::const_iterator UnifiedPollSet::end() const
{
    const_iterator iter;
    iter.m_impl = new ConstIteratorImpl(const_cast<UnifiedPollSet*>(this));
    iter.m_impl->currentSet = CurrentSet::sysWrite;
    iter.m_impl->sysSocketIter = m_writeSysFds.end();
    iter.m_impl->udtSocketIter = m_writeUdtFds.end();
    return iter;
}

template<typename SocketHandle>
bool UnifiedPollSet::addSocket(
    SocketHandle handle,
    int newEventMask,
    Pollable* socket,
    int(*addToPollSet)(int, SocketHandle, const int*),
    int(*removeFromPollSet)(int, SocketHandle),
    std::map<SocketHandle, SocketContext>* const socketDictionary)
{
    auto iterAndIsInsertedFlag =
        socketDictionary->emplace(handle, SocketContext{ newEventMask, nullptr });
    auto it = iterAndIsInsertedFlag.first;
    if (!iterAndIsInsertedFlag.second)
    {
        newEventMask |= it->second.eventMask;
        if (newEventMask == it->second.eventMask)
            return true;

        removeFromPollSet(m_epollFd, handle);
    }

    if (addToPollSet(m_epollFd, handle, &newEventMask) != 0)
    {
        socketDictionary->erase(it);
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }

    it->second.socket = socket;
    it->second.eventMask = newEventMask;
    return true;
}

template<typename SocketHandle>
bool UnifiedPollSet::removeSocket(
    UDTSOCKET handle,
    int eventToRemoveMask,
    int(*addToPollSet)(int, SocketHandle, const int*),
    int(*removeFromPollSet)(int, SocketHandle),
    std::map<SocketHandle, SocketContext>* const socketDictionary)
{
    auto it = socketDictionary->find(handle);
    Q_ASSERT(it != socketDictionary->end());
    if (it == socketDictionary->end())
        return true;

    int newEventMask = it->second.eventMask & (~eventToRemoveMask);
    if (newEventMask == it->second.eventMask)
        return true; //nothing to do

    if (removeFromPollSet(m_epollFd, handle) != 0)
        return false;

    if (newEventMask == 0)
    {
        socketDictionary->erase(it);
        return true;
    }

    //ignoring error since for udt sockets it fails if unrecoverable error happended to socket
        //TODO #ak But, this is, of course, not correct. MUST fix! Most likely, it will required AioThread modifications
    addToPollSet(m_epollFd, handle, &newEventMask);
    //if (addToPollSet(m_epollFd, handle, &newEventMask) != 0)
    //{
    //    socketDictionary->erase(it);
    //    SystemError::setLastErrorCode(
    //        detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    //    return false;
    //}

    it->second.eventMask = newEventMask;
    return true;
}

bool UnifiedPollSet::addUdtSocket(UDTSOCKET handle, int newEventMask, Pollable* socket)
{
    return addSocket(
        handle,
        newEventMask,
        socket,
        &UDT::epoll_add_usock,
        &UDT::epoll_remove_usock,
        &m_udtSockets);
}

bool UnifiedPollSet::removeUdtSocket(UDTSOCKET handle, int eventToRemoveMask)
{
    return removeSocket(
        handle,
        eventToRemoveMask,
        &UDT::epoll_add_usock,
        &UDT::epoll_remove_usock,
        &m_udtSockets);
}

bool UnifiedPollSet::addSysSocket(
    AbstractSocket::SOCKET_HANDLE handle,
    int newEventMask,
    Pollable* socket)
{
    return addSocket(
        handle,
        newEventMask,
        socket,
        &UDT::epoll_add_ssock,
        &UDT::epoll_remove_ssock,
        &m_sysSockets);
}

bool UnifiedPollSet::removeSysSocket(
    AbstractSocket::SOCKET_HANDLE handle,
    int eventToRemoveMask)
{
    return removeSocket(
        handle,
        eventToRemoveMask,
        &UDT::epoll_add_ssock,
        &UDT::epoll_remove_ssock,
        &m_sysSockets);
}

bool UnifiedPollSet::isElementBeingUsed(
    CurrentSet currentSet,
    UDTSOCKET handle) const
{
    for (auto iter : m_iterators)
    {
        if (iter->currentSet == currentSet &&
            *(iter->udtSocketIter) == handle)
        {
            //cannot remove element since it is being used
            return true;
        }
    }

    return false;
}

bool UnifiedPollSet::isElementBeingUsed(
    CurrentSet currentSet,
    AbstractSocket::SOCKET_HANDLE handle) const
{
    for (auto iter : m_iterators)
    {
        if (iter->currentSet == currentSet &&
            *(iter->sysSocketIter) == handle)
        {
            //cannot remove element since it is being used
            return true;
        }
    }

    return false;
}

void UnifiedPollSet::moveIterToTheNextEvent(ConstIteratorImpl* const iter) const
{
    for (int i = 0; ; ++i)
    {
        switch (iter->currentSet)
        {
            case CurrentSet::none:
                iter->currentSet = CurrentSet::udtRead;
                iter->udtSocketIter = m_readUdtFds.begin();
                continue;

            case CurrentSet::udtRead:
                if (i == 0)
                    ++(iter->udtSocketIter);
                if (iter->udtSocketIter == m_readUdtFds.end())
                {
                    iter->currentSet = CurrentSet::udtWrite;
                    iter->udtSocketIter = m_writeUdtFds.begin();
                    continue;
                }
                break;

            case CurrentSet::udtWrite:
                if (i == 0)
                    ++(iter->udtSocketIter);
                if (iter->udtSocketIter == m_writeUdtFds.end())
                {
                    iter->currentSet = CurrentSet::sysRead;
                    iter->sysSocketIter = m_readSysFds.begin();
                    continue;
                }
                break;

            case CurrentSet::sysRead:
                if (i == 0)
                    ++(iter->sysSocketIter);
                if (iter->sysSocketIter == m_readSysFds.end())
                {
                    iter->currentSet = CurrentSet::sysWrite;
                    iter->sysSocketIter = m_writeSysFds.begin();
                    continue;
                }
                break;

            case CurrentSet::sysWrite:
                if (i == 0)
                    ++(iter->sysSocketIter);
                break;
        }
        break;
    }
}

}   //aio
}   //network
}   //nx
