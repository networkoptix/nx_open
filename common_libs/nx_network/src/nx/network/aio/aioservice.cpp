/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aioservice.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

#include <nx/tool/thread/mutex.h>
#include <nx/tool/thread/mutex.h>
#include <QtCore/QThread>
#include <qglobal.h>

#include <nx/tool/log/log.h>


using namespace std;

namespace aio
{
    typedef AIOThread<Pollable> SystemAIOThread;

    static std::atomic<AIOService*> AIOService_instance( nullptr );

    AIOService::AIOService( unsigned int threadCount )
    {
        if( !threadCount )
            threadCount = QThread::idealThreadCount();

        initializeAioThreadPool(&m_systemSocketAIO, threadCount);
        initializeAioThreadPool(&m_udtSocketAIO, threadCount);

        assert(!m_systemSocketAIO.aioThreadPool.empty() && !m_udtSocketAIO.aioThreadPool.empty());
        if( m_systemSocketAIO.aioThreadPool.empty() && m_udtSocketAIO.aioThreadPool.empty() )
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
        m_systemSocketAIO.aioThreadPool.clear();

        m_udtSocketAIO.sockets.clear();
        m_udtSocketAIO.aioThreadPool.clear();
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

    template<> AIOService::SocketAIOContext<UdtSocket>& AIOService::getAIOHandlingContext()
    {
        return m_udtSocketAIO;
    }

    template<> const AIOService::SocketAIOContext<UdtSocket>& AIOService::getAIOHandlingContext() const
    {
        return m_udtSocketAIO;
    }
}
