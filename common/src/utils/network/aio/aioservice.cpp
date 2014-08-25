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
        for( auto thread : m_systemSocketAIO.aioThreadPool )
            delete thread;
        m_systemSocketAIO.aioThreadPool.clear();

        m_udtSocketAIO.sockets.clear();
        for( auto thread : m_udtSocketAIO.aioThreadPool )
            delete thread;
        m_udtSocketAIO;
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

    template<> AIOService::SocketAIOContext<UdtSocket>& AIOService::getAIOHandlingContext()
    {
        return m_udtSocketAIO;
    }

    template<> const AIOService::SocketAIOContext<UdtSocket>& AIOService::getAIOHandlingContext() const
    {
        return m_udtSocketAIO;
    }
}
