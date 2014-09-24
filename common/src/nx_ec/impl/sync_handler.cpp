/**********************************************************
* 27 jan 2014
* a.kolesnikov
***********************************************************/

#include "sync_handler.h"

#include <QtCore/QMutexLocker>


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
            QMutexLocker lk( &m_mutex );
            while( !m_done )
                m_cond.wait( lk.mutex() );
        }

        ErrorCode SyncHandler::errorCode() const
        {
            QMutexLocker lk( &m_mutex );
            return m_errorCode;
        }

        void SyncHandler::done( int /*reqID*/, ErrorCode _errorCode )
        {
            QMutexLocker lk( &m_mutex );
            m_done = true;
            m_errorCode = _errorCode;
            m_cond.wakeAll();
        }
    }
}
