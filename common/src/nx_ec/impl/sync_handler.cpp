/**********************************************************
* 27 jan 2014
* a.kolesnikov
***********************************************************/

#include "sync_handler.h"


namespace ec2
{
    SyncHandler::SyncHandler()
    :
        m_done( false ),
        m_errorCode( ErrorCode::ok )
    {
    }

    void SyncHandler::wait()
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_cond.wait( lk, [this]{return m_done;} );
    }

    ErrorCode SyncHandler::errorCode() const
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        return m_errorCode;
    }

    void SyncHandler::done( ErrorCode _errorCode )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_done = true;
        m_errorCode = _errorCode;
        m_cond.notify_all();
    }


    AbstractECConnectionPtr SyncConnectHandler::connection() const
    {
        return m_connection;
    }

    void SyncConnectHandler::done( ErrorCode errorCode, AbstractECConnectionPtr connection )
    {
        m_connection = connection;
        SyncHandler::done( errorCode );
    }
}
