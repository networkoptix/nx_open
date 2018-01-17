#pragma once

#include <map>
#include <set>

#include "abstract_pollset.h"
#include "event_type.h"
#include "nx/network/abstract_socket.h"
#include "nx/network/system_socket.h"

typedef int UDTSOCKET;

namespace nx {
namespace network {
namespace aio {

class ConstIteratorImpl;

enum class CurrentSet
{
    none,
    udtRead,
    udtWrite,
    sysRead,
    sysWrite
};

class NX_NETWORK_API AbstractUdtEpollWrapper
{
public:
    virtual ~AbstractUdtEpollWrapper() = default;

    /** Follows UDT::epoll_wait API. */
    virtual int epollWait(
        int epollFd,
        std::map<UDTSOCKET, int>* readReadyUdtSockets,
        std::map<UDTSOCKET, int>* writeReadyUdtSockets,
        int64_t timeoutMillis,
        std::map<AbstractSocket::SOCKET_HANDLE, int>* readReadySystemSockets,
        std::map<AbstractSocket::SOCKET_HANDLE, int>* writeReadySystemSockets) = 0;
};

class NX_NETWORK_API UnifiedPollSet
{
    friend class ConstIteratorImpl;

public:
    class NX_NETWORK_API const_iterator
    {
        friend class UnifiedPollSet;

    public:
        const_iterator();
        const_iterator(const_iterator&&);
        ~const_iterator();

        const_iterator& operator=(const_iterator&&);
        const_iterator operator++(int);     //it++
        const_iterator& operator++();       //++it

        Pollable* socket();
        const Pollable* socket() const;
        aio::EventType eventType() const;

        bool operator==(const const_iterator& right) const;
        bool operator!=(const const_iterator& right) const;

    private:
        ConstIteratorImpl* m_impl;

        const_iterator(const const_iterator&);
        const_iterator& operator=(const const_iterator&);
    };

    UnifiedPollSet();
    UnifiedPollSet(std::unique_ptr<AbstractUdtEpollWrapper>);
    virtual ~UnifiedPollSet();

    /**
     * Returns true, if all internal data has been initialized successfully.
     */
    bool isValid() const;

    /**
     * Interrupts poll method, blocked in other thread.
     * This is the only method which is allowed to be called from different thread.
     * poll, called after interrupt, will return immediately.
     * But, it is unspecified whether it will return
     * multiple times if interrupt has been called multiple times.
     */
    void interrupt();

    /**
     * Add socket to set.
     * @param userData This value is associated with pair (socket, eventType)
     * @return true, if socket added to set
     * NOTE: This method does not check, whether socket is already in pollset.
     * NOTE: Ivalidates all iterators.
     */
    bool add(Pollable* const sock, EventType eventType, void* userData = NULL);
    /**
     * NOTE: Ivalidates all iterators to the left of removed element.
     * So, it is ok to iterate signalled sockets and remove current element.
     * Subsequent iterator increment operation will perform safely.
     */
    void remove(Pollable* const sock, EventType eventType);
    size_t size() const;
    /**
     * @param millisToWait if 0, method returns immediatly (useful to test socket state).
     * @return -1 on error, 0 if timeout has expired, > 0 - number of socket whose state has been changed
     * NOTE: If multiple event occured on same socket each event
     * will be present as a single element when iterating.
     * NOTE: Invalidates iterators.
     */
    int poll(int millisToWait = kInfiniteTimeout);
    int poll(std::chrono::milliseconds timeout) { return poll(timeout.count()); }

    /**
     * @return Iterator pointing to the first socket,
     * which state has been changed in previous UnifiedPollSet::poll call.
     */
    const_iterator begin() const;
    /**
     * Returns iterator pointing just beyond last socket, which state has been changed in previous UnifiedPollSet::poll call.
     */
    const_iterator end() const;

private:
    struct SocketContext
    {
        int eventMask;
        Pollable* socket;
    };

    int m_epollFd;
    std::map<UDTSOCKET, SocketContext> m_udtSockets;
    std::map<AbstractSocket::SOCKET_HANDLE, SocketContext> m_sysSockets;

    std::map<UDTSOCKET, int> m_readUdtFds;
    std::map<UDTSOCKET, int> m_writeUdtFds;
    std::map<AbstractSocket::SOCKET_HANDLE, int> m_readSysFds;
    std::map<AbstractSocket::SOCKET_HANDLE, int> m_writeSysFds;

    std::unique_ptr<AbstractUdtEpollWrapper> m_udtEpollWrapper;

    template<typename SocketHandle>
    bool addSocket(
        SocketHandle handle,
        int newEventMask,
        Pollable* socket,
        int(*addToPollSet)(int, SocketHandle, const int*),
        int(*removeFromPollSet)(int, SocketHandle),
        std::map<SocketHandle, SocketContext>* const socketDictionary);
    template<typename SocketHandle>
    bool removeSocket(
        SocketHandle handle,
        int eventToRemoveMask,
        int(*addToPollSet)(int, SocketHandle, const int*),
        int(*removeFromPollSet)(int, SocketHandle),
        std::map<SocketHandle, SocketContext>* const socketDictionary);

    bool addUdtSocket(UDTSOCKET handle, int newEventMask, Pollable* socket);
    bool removeUdtSocket(UDTSOCKET handle, int eventToRemoveMask);
    bool addSysSocket(AbstractSocket::SOCKET_HANDLE handle, int newEventMask, Pollable* socket);
    bool removeSysSocket(AbstractSocket::SOCKET_HANDLE handle, int eventToRemoveMask);
    bool isUdtElementBeingUsed(
        CurrentSet currentSet,
        UDTSOCKET handle) const;
    bool isSysElementBeingUsed(
        CurrentSet currentSet,
        AbstractSocket::SOCKET_HANDLE handle) const;
    std::set<ConstIteratorImpl*> m_iterators;
    UDPSocket m_interruptSocket;

    void moveIterToTheNextEvent(ConstIteratorImpl* const iter) const;
    void removePhantomSockets(std::map<UDTSOCKET, int>* const udtFdSet);
};

} // namespace aio
} // namespace network
} // namespace nx
