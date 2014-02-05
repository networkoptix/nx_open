/**********************************************************
* 27 jan 2014
* a.kolesnikov
***********************************************************/

#include "sync_handler.h"


namespace ec2
{
    namespace impl
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

        void SyncHandler::done( int reqID, ErrorCode _errorCode )
        {
            std::unique_lock<std::mutex> lk( m_mutex );
            m_done = true;
            m_errorCode = _errorCode;
            m_cond.notify_all();
        }
    }
}
