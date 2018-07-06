#include "unified_pollset.h"

#include <iostream>

#include <nx/utils/std/cpp14.h>
#include <udt/udt.h>

#include "../udt/udt_common.h"
#include "../udt/udt_socket.h"
#include "../udt/udt_socket_impl.h"

namespace nx {
namespace network {
namespace aio {

namespace {

class UdtEpollWrapper:
    public AbstractUdtEpollWrapper
{
public:
    virtual int epollWait(
        int epollFd,
        std::map<UDTSOCKET, int>* readReadyUdtSockets,
        std::map<UDTSOCKET, int>* writeReadyUdtSockets,
        int64_t timeoutMillis,
        std::map<AbstractSocket::SOCKET_HANDLE, int>* readReadySystemSockets,
        std::map<AbstractSocket::SOCKET_HANDLE, int>* writeReadySystemSockets) override
    {
        return UDT::epoll_wait(
            epollFd,
            readReadyUdtSockets,
            writeReadyUdtSockets,
            timeoutMillis,
            readReadySystemSockets,
            writeReadySystemSockets);
    }
};

} // namespace

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
            NX_ASSERT(0);
            return 0;
    }
}

} // namespace

/*********************************
ConstIteratorImpl
**********************************/
class ConstIteratorImpl
{
public:
    UnifiedPollSet* pollSet;
    CurrentSet currentSet;
    std::map<UDTSOCKET, int>::const_iterator udtSocketIter;
    bool udtSocketIterAtEnd;
    std::map<AbstractSocket::SOCKET_HANDLE, int>::const_iterator sysSocketIter;
    bool sysSocketIterAtEnd;

    ConstIteratorImpl(UnifiedPollSet* _pollSet):
        pollSet(_pollSet),
        currentSet(CurrentSet::none),
        udtSocketIterAtEnd(false),
        sysSocketIterAtEnd(false)
    {
        pollSet->m_iterators.insert(this);
    }

    ConstIteratorImpl(const ConstIteratorImpl&) = delete;
    ConstIteratorImpl& operator=(const ConstIteratorImpl&) = delete;
    ConstIteratorImpl(ConstIteratorImpl&&) = delete;
    ConstIteratorImpl& operator=(ConstIteratorImpl&&) = delete;

    ~ConstIteratorImpl()
    {
        const auto elementsRemoved = pollSet->m_iterators.erase(this);
        NX_ASSERT(elementsRemoved == 1);
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
        {
            const auto it = m_impl->pollSet->m_udtSockets.find(m_impl->udtSocketIter->first);
            NX_CRITICAL(it != m_impl->pollSet->m_udtSockets.end());
            return it->second.socket;
        }
        case CurrentSet::sysRead:
        case CurrentSet::sysWrite:
        {
            const auto it = m_impl->pollSet->m_sysSockets.find(m_impl->sysSocketIter->first);
            NX_CRITICAL(it != m_impl->pollSet->m_sysSockets.end());
            return it->second.socket;
        }
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
        {
            const auto it = m_impl->pollSet->m_udtSockets.find(m_impl->udtSocketIter->first);
            NX_CRITICAL(it != m_impl->pollSet->m_udtSockets.end());
            return it->second.socket;
        }
        case CurrentSet::sysRead:
        case CurrentSet::sysWrite:
        {
            const auto it = m_impl->pollSet->m_sysSockets.find(m_impl->sysSocketIter->first);
            NX_CRITICAL(it != m_impl->pollSet->m_sysSockets.end());
            return it->second.socket;
        }
        default:
            return nullptr;
    }
}

aio::EventType UnifiedPollSet::const_iterator::eventType() const
{
    switch (m_impl->currentSet)
    {
        case CurrentSet::udtRead:
            if (m_impl->udtSocketIter->second & UDT_EPOLL_ERR)
                return aio::etError;
            return aio::etRead;

        case CurrentSet::sysRead:
            if (m_impl->sysSocketIter->second & UDT_EPOLL_ERR)
                return aio::etError;
            return aio::etRead;

        case CurrentSet::udtWrite:
            if (m_impl->udtSocketIter->second & UDT_EPOLL_ERR)
                return aio::etError;
            return aio::etWrite;

        case CurrentSet::sysWrite:
            if (m_impl->sysSocketIter->second & UDT_EPOLL_ERR)
                return aio::etError;
            return aio::etWrite;

        default:
            NX_ASSERT(false);
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

UnifiedPollSet::UnifiedPollSet():
    UnifiedPollSet(nullptr)
{
}

UnifiedPollSet::UnifiedPollSet(std::unique_ptr<AbstractUdtEpollWrapper> udtEpollWrapper):
    m_epollFd(-1),
    m_udtEpollWrapper(std::move(udtEpollWrapper))
{
    if (!m_udtEpollWrapper)
        m_udtEpollWrapper = std::make_unique<UdtEpollWrapper>();

    m_epollFd = UDT::epoll_create();
}

UnifiedPollSet::~UnifiedPollSet()
{
    UDT::epoll_release(m_epollFd);
}

bool UnifiedPollSet::isValid() const
{
    return m_epollFd != -1;
}

void UnifiedPollSet::interrupt()
{
    UDT::epoll_interrupt_wait(m_epollFd);
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
            !isUdtElementBeingUsed(CurrentSet::udtRead, udtHandle))
        {
            m_readUdtFds.erase(udtHandle);
        }
        if ((udtEvent & UDT_EPOLL_OUT) > 0 &&
            !isUdtElementBeingUsed(CurrentSet::udtWrite, udtHandle))
        {
            m_writeUdtFds.erase(udtHandle);
        }
    }
    else
    {
        removeSysSocket(sock->handle(), udtEvent);

        if ((udtEvent & UDT_EPOLL_IN) > 0 &&
            !isSysElementBeingUsed(CurrentSet::sysRead, sock->handle()))
        {
            m_readSysFds.erase(sock->handle());
        }
        if ((udtEvent & UDT_EPOLL_OUT) > 0 &&
            !isSysElementBeingUsed(CurrentSet::sysWrite, sock->handle()))
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

    int result = m_udtEpollWrapper->epollWait(
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

    removePhantomSockets(&m_readUdtFds);
    removePhantomSockets(&m_writeUdtFds);

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
    iter.m_impl->sysSocketIterAtEnd = true;
    iter.m_impl->udtSocketIter = m_writeUdtFds.end();
    iter.m_impl->udtSocketIterAtEnd = true;
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
    SocketHandle handle,
    int eventToRemoveMask,
    int(*addToPollSet)(int, SocketHandle, const int*),
    int(*removeFromPollSet)(int, SocketHandle),
    std::map<SocketHandle, SocketContext>* const socketDictionary)
{
    auto it = socketDictionary->find(handle);
    //NX_ASSERT(it != socketDictionary->end());
    if (it == socketDictionary->end())
        return true;    //for now, this is valid case if adding to pollset has failed

    int newEventMask = it->second.eventMask & (~eventToRemoveMask);
    if (newEventMask == it->second.eventMask)
        return true; //nothing to do

    if (removeFromPollSet(m_epollFd, handle) != 0)
    {
        //return false;
    }

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

bool UnifiedPollSet::isUdtElementBeingUsed(
    CurrentSet currentSet,
    UDTSOCKET handle) const
{
    for (auto iter: m_iterators)
    {
        if (iter->currentSet == currentSet &&
            !iter->udtSocketIterAtEnd &&
            iter->udtSocketIter->first == handle)
        {
            //cannot remove element since it is being used
            return true;
        }
    }

    return false;
}

bool UnifiedPollSet::isSysElementBeingUsed(
    CurrentSet currentSet,
    AbstractSocket::SOCKET_HANDLE handle) const
{
    for (auto iter: m_iterators)
    {
        if (iter->currentSet == currentSet &&
            !iter->sysSocketIterAtEnd &&
            iter->sysSocketIter->first == handle)
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
                    iter->udtSocketIterAtEnd = true;
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
                iter->sysSocketIterAtEnd = iter->sysSocketIter == m_writeSysFds.end();
                break;
        }
        break;
    }
}

void UnifiedPollSet::removePhantomSockets(std::map<UDTSOCKET, int>* const udtFdSet)
{
    // TODO: #ak Make O(N) here instead of current O(N*log(N)).
    for (auto it = udtFdSet->begin(); it != udtFdSet->end();)
    {
        if (m_udtSockets.find(it->first) == m_udtSockets.end())
            it = udtFdSet->erase(it);   // UDT sometimes reports phantom FD.
        else
            ++it;
    }
}

} // namespace aio
} // namespace network
} // namespace nx
