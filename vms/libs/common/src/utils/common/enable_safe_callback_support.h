/**********************************************************
* 23 dec 2014
* a.kolesnikov
***********************************************************/

#ifndef ENABLE_SAFE_CALLBACK_SUPPORT_H
#define ENABLE_SAFE_CALLBACK_SUPPORT_H

#include <condition_variable>
#include <mutex>


//!This class is to be used to help to subscribe/unsubscribe/deliver direct events to objects living in different thread
template<class EventReceiverType>
class EnableSafeCallbackSupport
{
public:
    EnableSafeCallbackSupport()
    :
        m_receiverUsed( -1 ),
        m_prevGivenReceiverID( 0 )
    {
    }

    /*!
        \return receiver ID. Always greater than zero
    */
    template<class EventReceiverRefType>
    qint64 registerReceiver( EventReceiverRefType&& receiver )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        ++m_prevGivenReceiverID;
        if( m_prevGivenReceiverID == std::numeric_limits<qint64>::max() )
            m_prevGivenReceiverID = 1;
        m_receivers[m_prevGivenReceiverID] = std::forward<EventReceiverRefType>(receiver);
        return m_prevGivenReceiverID;
    }

    //!If callback is being executed in another thread, then this method blocks until execution is done and then unregisters receiver
    /*!
        \param receiverID Returned by \a EnableSafeCallbackSupport::registerReceiver
    */
    void unregisterReceiverAndJoin( qint64 receiverID )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        while( m_receiverUsed == receiverID )
            m_cond.wait( lk );
        //removing callback
        m_receivers.erase( receiverID );
    }

    //!Executes \a func for each registered receiver
    /*!
        New receivers, registered within callback, do not receive this event
    */
    template<class CallbackFunc>
    void executeCallback( CallbackFunc func )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        if( m_receivers.empty() )
            return;

        const qint64 maxReceiverIDToCall = m_receivers.rbegin()->first;
        for( auto
            it = m_receivers.cbegin();
            (it != m_receivers.cend()) &&
                (it->first <= maxReceiverIDToCall); //newly-registered receivers will not be called
            ++it )
        {
            auto idAndReceiver = *it;   //TODO #ak copying receiver is not good
            m_receiverUsed = idAndReceiver.first;
            lk.unlock();
            func( idAndReceiver.second );
            lk.lock();
            m_receiverUsed = -1;
            m_cond.notify_all();
        }
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::map<qint64, EventReceiverType> m_receivers;
    qint64 m_receiverUsed;
    qint64 m_prevGivenReceiverID;
};

#endif  //ENABLE_SAFE_CALLBACK_SUPPORT_H
