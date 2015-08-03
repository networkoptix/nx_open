/**********************************************************
* Jul 17, 2015
* a.kolesnikov
***********************************************************/

#include "safe_direct_connection.h"

#include <atomic>


namespace Qn
{
    ////////////////////////////////////////////////////////////
    //// class EnableSafeDirectConnection
    ////////////////////////////////////////////////////////////

    static std::atomic<EnableSafeDirectConnection::ID> enableSafeDirectConnectionCounter( 0 );

    EnableSafeDirectConnection::EnableSafeDirectConnection()
    :
        m_globalHelper( SafeDirectConnectionGlobalHelper::instance() ),
        m_uniqueObjectSequence( ++enableSafeDirectConnectionCounter )
    {
    }

    EnableSafeDirectConnection::~EnableSafeDirectConnection()
    {
        //TODO #ak verifying that safeDirectDisconnect() has been called
        Q_ASSERT( !m_globalHelper->isConnected(this) );
    }

    void EnableSafeDirectConnection::directDisconnectAll()
    {
        m_globalHelper->directDisconnectAll( this );
    }

    EnableSafeDirectConnection::ID EnableSafeDirectConnection::uniqueObjectSequence() const
    {
        return m_uniqueObjectSequence;
    }


    ////////////////////////////////////////////////////////////
    //// class SafeDirectConnectionGlobalHelper
    ////////////////////////////////////////////////////////////

    void SafeDirectConnectionGlobalHelper::directDisconnectAll( const EnableSafeDirectConnection* receiver )
    {
        //waiting for all slots to return
        QMutexLocker lk( &m_mutex );
        std::map<EnableSafeDirectConnection::ID, ReceiverContext>::iterator it;
        for( ;; )
        {
            it = m_receivers.find( receiver->uniqueObjectSequence() );
            if( it == m_receivers.end() )
                return;
            it->second.terminated = true;   //forbidding to call new slots for this method to be finite
            if( it->second.slotsInvokedCounter == 0 )
                break;
            m_cond.wait( lk.mutex() );
        }

        //disconnecting
        for( const QMetaObject::Connection& connection: it->second.connections )
            QObject::disconnect( connection );
        m_receivers.erase( it );
    }

    bool SafeDirectConnectionGlobalHelper::isConnected( const EnableSafeDirectConnection* receiver ) const
    {
        QMutexLocker lk( &m_mutex );
        return m_receivers.find( receiver->uniqueObjectSequence() ) != m_receivers.end();
    }

    //TODO #ak create this instance on-demand
    static std::shared_ptr<SafeDirectConnectionGlobalHelper>
        safeDirectConnectionGlobalHelperInstance( std::make_shared<SafeDirectConnectionGlobalHelper>() );

    std::shared_ptr<SafeDirectConnectionGlobalHelper> SafeDirectConnectionGlobalHelper::instance()
    {
        return safeDirectConnectionGlobalHelperInstance;
    }

    void SafeDirectConnectionGlobalHelper::newSafeConnectionEstablished(
        EnableSafeDirectConnection::ID receiver,
        QMetaObject::Connection connection )
    {
        QMutexLocker lk( &m_mutex );
        m_receivers[receiver].connections.emplace_back( connection );
    }

    bool SafeDirectConnectionGlobalHelper::beforeSlotInvoked(
        const QObject* /*sender*/,
        EnableSafeDirectConnection::ID receiver )
    {
        QMutexLocker lk( &m_mutex );
        auto it = m_receivers.find( receiver );
        if( it == m_receivers.end() || it->second.terminated )
            return false;
        ++it->second.slotsInvokedCounter;
        return true;
    }
    
    void SafeDirectConnectionGlobalHelper::afterSlotInvoked(
        const QObject* /*sender*/,
        EnableSafeDirectConnection::ID receiver )
    {
        QMutexLocker lk( &m_mutex );
        auto it = m_receivers.find( receiver );
        if( it == m_receivers.end() )
            return;
        Q_ASSERT( it->second.slotsInvokedCounter > 0 );
        --it->second.slotsInvokedCounter;
        if( it->second.slotsInvokedCounter == 0 )
        {
            lk.unlock();
            m_cond.wakeAll();
        }
    }


    void directDisconnectAll( const EnableSafeDirectConnection* receiver )
    {
        SafeDirectConnectionGlobalHelper::instance()->directDisconnectAll( receiver );
    }
}
