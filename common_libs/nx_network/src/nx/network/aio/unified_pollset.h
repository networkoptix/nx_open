/**********************************************************
* Feb 25, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <map>
#include <set>

#include "pollset.h"
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
    virtual ~UnifiedPollSet();

    //!Returns true, if all internal data has been initialized successfully
    bool isValid() const;

    //!Interrupts \a poll method, blocked in other thread
    /*!
        This is the only method which is allowed to be called from different thread.
        poll, called after interrupt, will return immediately. But, it is unspecified whether it will return multiple times if interrupt has been called multiple times
    */
    void interrupt();

    //!Add socket to set. Does not take socket ownership
    /*!
        \param eventType event to monitor on socket \a sock
        \param userData
        \return true, if socket added to set
        \note This method does not check, whether \a sock is already in pollset
        \note Ivalidates all iterators
        \note \a userData is associated with pair (\a sock, \a eventType)
    */
    bool add(Pollable* const sock, EventType eventType, void* userData = NULL);
    //!Do not monitor event \a eventType on socket \a sock anymore
    /*!
        \note Ivalidates all iterators to the left of removed element. So, it is ok to iterate signalled sockets and remove current element:
        following iterator increment operation will perform safely
    */
    void remove(Pollable* const sock, EventType eventType);
    //!Returns number of sockets in pollset
    /*!
        Returned value should only be used for comparision against size of another \a UnifiedPollSet instance
    */
    size_t size() const;
    /*!
        \param millisToWait if 0, method returns immediatly. If > 0, returns on event or after \a millisToWait milliseconds.
        If < 0, method blocks till event
        \return -1 on error, 0 if \a millisToWait timeout has expired, > 0 - number of socket whose state has been changed
        \note If multiple event occured on same socket each event will be present as a single element
    */
    int poll(int millisToWait = INFINITE_TIMEOUT);

    //!Returns iterator pointing to first socket, which state has been changed in previous \a poll call
    const_iterator begin() const;
    //!Returns iterator pointing to next element after last socket, which state has been changed in previous \a poll call
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

    std::set<UDTSOCKET> m_readUdtFds;
    std::set<UDTSOCKET> m_writeUdtFds;
    std::set<AbstractSocket::SOCKET_HANDLE> m_readSysFds;
    std::set<AbstractSocket::SOCKET_HANDLE> m_writeSysFds;

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
    void removePhantomSockets(std::set<UDTSOCKET>* const udtFdSet);
};

}   //aio
}   //network
}   //nx
