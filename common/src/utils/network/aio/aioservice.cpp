/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aioservice.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

#include <utils/common/mutex.h>
#include <utils/common/mutex.h>
#include <QtCore/QThread>
#include <qglobal.h>

#include <utils/common/log.h>


using namespace std;

namespace aio
{
    typedef AIOThread<Pollable> SystemAIOThread;

    static std::atomic<AIOService*> AIOService_instance( nullptr );

    AIOService::AIOService( unsigned int threadCount )
    {
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

        Q_ASSERT( AIOService_instance.load() == nullptr );
        AIOService_instance = this;
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

        Q_ASSERT( AIOService_instance == this );
        AIOService_instance = nullptr;
    }

    AIOService* AIOService::instance()
    {
        SocketGlobalRuntime::instance();    //instanciates socket-related globals in correct order
        return AIOService_instance.load( std::memory_order_relaxed );
    }

    //!Returns true, if object has been successfully initialized
    bool AIOService::isInitialized() const
    {
        return !m_systemSocketAIO.aioThreadPool.empty();
    }

    template<> AIOService::SocketAIOContext<Pollable>& AIOService::getAIOHandlingContext()
    {
        return m_systemSocketAIO;
    }

    template<> const AIOService::SocketAIOContext<Pollable>& AIOService::getAIOHandlingContext() const
    {
        return m_systemSocketAIO;
    }

}
