#pragma once

#include <cstddef>

#include "event_type.h"

namespace nx {
namespace network {

class Pollable;

namespace aio {

class PollSetImpl;
class ConstIteratorImplOld;

/** NOTE: Do not wait more than a second for posted calls in case of interupt failure. */
static const int INFINITE_TIMEOUT = -1;

//!Allows to wait for state change on mutiple sockets
/*!
    If \a poll() returns positive value, then \a begin() returns iterator to first socket which state has been changed.
    Every socket is always monitored for error and all errors are reported.
    \note This class is not thread-safe
    \note If multiple event occured on same socket each event will be presented separately
    \note Polling same socket with two \a PollSet instances results in undefined behavior
*/
class NX_NETWORK_API PollSet
{
public:
    /*!
        Using iterator in other thread than \a poll() results in undefined behavour.
        \note If element, iterator is pointing to, has been removed, it is still safe to increment iterator
    */
    class NX_NETWORK_API const_iterator
    {
        friend class PollSet;

    public:
        const_iterator();
        const_iterator( const const_iterator& );
        ~const_iterator();

        const_iterator& operator=( const const_iterator& );
        //!Selects next socket which state has been changed with previous \a poll call
        /*!
            \note If iterator points to removed element, it is safe to increment iterator
        */
        const_iterator operator++(int);    //it++
        //!Selects next socket which state has been changed with previous \a poll call
        /*!
            \note If iterator points to removed element, it is safe to increment iterator
        */
        const_iterator& operator++();       //++it

        Pollable* socket();
        const Pollable* socket() const;
        /*!
            \return Triggered event
        */
        aio::EventType eventType() const;
        void* userData();

        bool operator==( const const_iterator& right ) const;
        bool operator!=( const const_iterator& right ) const;

    private:
        ConstIteratorImplOld* m_impl;
    };

    PollSet();
    virtual ~PollSet();

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
    bool add( Pollable* const sock, EventType eventType, void* userData = NULL );
    //!Do not monitor event \a eventType on socket \a sock anymore
    /*!
        \note Ivalidates all iterators to the left of removed element. So, it is ok to iterate signalled sockets and remove current element:
            following iterator increment operation will perform safely
    */
    void remove( Pollable* const sock, EventType eventType );
    //!Returns number of sockets in pollset
    /*!
        Returned value should only be used for comparision against size of another \a PollSet instance
    */
    size_t size() const;
    /*!
        \param millisToWait if 0, method returns immediatly. If > 0, returns on event or after \a millisToWait milliseconds.
            If < 0, method blocks till event
        \return -1 on error, 0 if \a millisToWait timeout has expired, > 0 - number of socket whose state has been changed
        \note If multiple event occured on same socket each event will be present as a single element
    */
    int poll( int millisToWait = INFINITE_TIMEOUT );

    //!Returns iterator pointing to first socket, which state has been changed in previous \a poll call
    const_iterator begin() const;
    //!Returns iterator pointing to next element after last socket, which state has been changed in previous \a poll call
    const_iterator end() const;

private:
    PollSetImpl* m_impl;
};

} // namespace aio
} // namespace network
} // namespace nx
