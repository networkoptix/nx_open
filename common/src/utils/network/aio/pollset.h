/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef POLLSET_H
#define POLLSET_H

#include <cstddef>


class Socket;

namespace aio
{
    class PollSetImpl;
    class ConstIteratorImpl;

    enum EventType
    {
        etNone = 0,
        etRead = 1,
        etWrite = 2,
        //!Error occured on socket. Output only event. To get socket error code use \a Socket::getLastError
        etError = 4,
        //!Used for periodic operations and for socket timers
        etTimedOut = 8,
        etReadTimedOut = etRead | etTimedOut,
        etWriteTimedOut = etWrite | etTimedOut,
        etMax = 9
    };

    const char* toString( EventType eventType );

    static const int INFINITE_TIMEOUT = -1;

    //!Allows to wait for state change on mutiple sockets
    /*!
        If \a poll() returns positive value, then \a begin() returns iterator to first socket which state has been changed.
        Every socket is always monitored for error and all errors are reported.
        \note This class is not thread-safe
        \note If multiple event occured on same socket each event will be presented separately
        \note Polling same socket with two \a PollSet instances results in undefined behavior
    */
    class PollSet
    {
    public:
        /*!
            Using iterator in other thread than \a poll() results in undefined behavour
        */
        class const_iterator
        {
            friend class PollSet;

        public:
            const_iterator();
            const_iterator( const const_iterator& );
            ~const_iterator();

            const_iterator& operator=( const const_iterator& );
            //!Selects next socket which state has been changed with previous \a poll call
            const_iterator operator++(int);    //it++
            //!Selects next socket which state has been changed with previous \a poll call
            const_iterator& operator++();       //++it

            Socket* socket();
            const Socket* socket() const;
            /*!
                \return Triggered event
            */
            aio::EventType eventType() const;
            void* userData();

            bool operator==( const const_iterator& right ) const;
            bool operator!=( const const_iterator& right ) const;

        private:
            ConstIteratorImpl* m_impl;
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
        bool add( Socket* const sock, EventType eventType, void* userData = NULL );
        //!Do not monitor event \a eventType on socket \a sock anymore
        /*!
            \note Ivalidates all iterators to the left of removed element. So, it is ok to iterate signalled sockets and remove current element
        */
        void remove( Socket* const sock, EventType eventType );
        //!Returns number of sockets in pollset
        /*!
            Returned value should only be used for compare with \a maxPollSetSize()
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

        //!Returns maximum possible poll set size (depends on OS)
        static unsigned int maxPollSetSize();

    private:
        PollSetImpl* m_impl;
    };
}

#endif  //POLLSET_H
