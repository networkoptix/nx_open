/**********************************************************
* 28 oct 2014
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef WAITABLE_QUEUE_WITH_EVENTFD_H
#define WAITABLE_QUEUE_WITH_EVENTFD_H

#include <sys/eventfd.h>
#include <unistd.h>

#include <deque>
#include <mutex>


//!Queue with new element notification. Notification is delivered by signalling underlying linux event FD
/*!
    \note Since this class uses eventfd, it is linux-specific
    \note If \a WaitableQueueWithEventFD::eventFD is -1, object cannot be used
*/
template<class T>
class WaitableQueueWithEventFD
{
public:
    //!Initialization
    /*!
        After object instanciation user MUST call \a WaitableQueueWithEventFD::eventFD 
        to check whether object has been successfully initialized
    */
    WaitableQueueWithEventFD()
    :
        m_eventFD( eventfd( 0, EFD_SEMAPHORE ) )
    {
    }

    ~WaitableQueueWithEventFD()
    {
        if( m_eventFD != -1 )
        {
            ::close( m_eventFD );
            m_eventFD = -1;
        }
    }

    bool pop( T* const data )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        uint64_t decrement = 0;
        if( read( m_eventFD, &decrement, sizeof(decrement) ) == -1 )
            return false;
        *data = std::move(m_data.front());
        m_data.pop_front();
        return true;
    }
    
    bool push( const T& newElement )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        uint64_t increment = 1;
        if( write( m_eventFD, &increment, sizeof(increment) ) == -1 )
            return false;
        m_data.push_back( newElement );
        return true;
    }

    //!Returns corresponding eventfd. -1, if failed to initialize
    int eventFD() const
    {
        return m_eventFD;
    }

private:
    int m_eventFD;
    std::mutex m_mutex;
    std::deque<T> m_data;
};

#endif  //WAITABLE_QUEUE_WITH_EVENTFD_H
