/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aioservice.h"

#include <iostream>
#include <memory>
#include <thread>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <qglobal.h>

#include <utils/common/log.h>


using namespace std;

namespace aio
{
    static unsigned int threadCountArgValue = 0;

    typedef AIOThread<Socket> SystemAIOThread;

    AIOService::AIOService( unsigned int threadCount )
    {
        if( !threadCount )
            threadCount = threadCountArgValue;

        if( !threadCount )
            threadCount = QThread::idealThreadCount();

        for( unsigned int i = 0; i < threadCount; ++i )
        {
            std::unique_ptr<SystemAIOThread> thread( new SystemAIOThread( &m_mutex ) );
            thread->start();
            if( !thread->isRunning() )
                continue;
            m_systemSocketAIO.aioThreadPool.push_back( thread.release() );
        }

        assert( !m_systemSocketAIO.aioThreadPool.empty() );
        if( m_systemSocketAIO.aioThreadPool.empty() )
        {
            //I wish we used exceptions
            NX_LOG( lit("Could not start a single AIO thread. Terminating..."), cl_logALWAYS );
            std::cerr << "Could not start a single AIO thread. Terminating..." << std::endl;
            exit(1);
        }
    }

    AIOService::~AIOService()
    {
        m_systemSocketAIO.sockets.clear();
        for( std::list<SystemAIOThread*>::iterator
            it = m_systemSocketAIO.aioThreadPool.begin();
            it != m_systemSocketAIO.aioThreadPool.end();
            ++it )
        {
            delete *it;
        }
            m_systemSocketAIO.aioThreadPool.clear();
    }

    //!Returns true, if object has been successfully initialized
    bool AIOService::isInitialized() const
    {
        return !m_systemSocketAIO.aioThreadPool.empty();
    }

    Q_GLOBAL_STATIC( AIOService, aioServiceInstance )
    
    AIOService* AIOService::instance( unsigned int threadCount )
    {
        threadCountArgValue = threadCount;
        return aioServiceInstance();
    }

    template<> AIOService::SocketAIOContext<Socket>& AIOService::getAIOHandlingContext()
    {
        return m_systemSocketAIO;
    }

    template<> const AIOService::SocketAIOContext<Socket>& AIOService::getAIOHandlingContext() const
    {
        return m_systemSocketAIO;
    }

}
