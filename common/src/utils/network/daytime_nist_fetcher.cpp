/**********************************************************
* 10 jul 2014
* a.kolesnikov
***********************************************************/

#include "daytime_nist_fetcher.h"

#include <sys/timeb.h>
#include <sys/types.h>

#include <QtCore/QMutexLocker>

#include "utils/common/log.h"
#include "utils/network/socket_factory.h"
#include "utils/tz/tz.h"


//TODO #ak try multiple servers in case of error or empty string (it happens pretty often)

static const unsigned int SECONDS_FROM_1900_01_01_TO_1970_01_01 = 2208988800L;
static const size_t MAX_TIME_STR_LENGTH = sizeof(time_t); 
static const char* DEFAULT_NIST_SERVER = "time.nist.gov";
static const unsigned short DAYTIME_PROTOCOL_DEFAULT_PORT = 37;     //time protocol
static const int SOCKET_READ_TIMEOUT = 7000;
static const int MILLIS_PER_SEC = 1000;
static const int SEC_PER_MIN = 60;

DaytimeNISTFetcher::DaytimeNISTFetcher()
{
    m_timeStr.reserve( MAX_TIME_STR_LENGTH );
}

DaytimeNISTFetcher::~DaytimeNISTFetcher()
{
    pleaseStop();
    join();
}

void DaytimeNISTFetcher::pleaseStop()
{
    //assuming that user stopped using object and DaytimeNISTFetcher::getTimeAsync will never be called,
        //so we can safely use m_tcpSock
    std::shared_ptr<AbstractStreamSocket> tcpSock;
    {
        QMutexLocker lk( &m_mutex );
        if( !m_tcpSock )
            return;
        tcpSock = m_tcpSock;
    }

    tcpSock->terminateAsyncIO( true );
    m_tcpSock.reset();
}

void DaytimeNISTFetcher::join()
{
}

bool DaytimeNISTFetcher::getTimeAsync( std::function<void(qint64, SystemError::ErrorCode)> handlerFunc )
{
    NX_LOG( lit( "NIST time_sync. Starting time synchronization with NIST server %1:%2" ).
        arg( QLatin1String( DEFAULT_NIST_SERVER ) ).arg( DAYTIME_PROTOCOL_DEFAULT_PORT ), cl_logDEBUG2 );

    {
        QMutexLocker lk( &m_mutex );
        m_tcpSock.reset( SocketFactory::createStreamSocket( false, SocketFactory::nttDisabled ) );
        if( !m_tcpSock )
            return false;
    }

    if( !m_tcpSock->setNonBlockingMode( true ) ||
        !m_tcpSock->setRecvTimeout(SOCKET_READ_TIMEOUT) ||
        !m_tcpSock->setSendTimeout(SOCKET_READ_TIMEOUT) )
    {
        m_tcpSock->terminateAsyncIO( true );
        return false;
    }

    m_handlerFunc = [this, handlerFunc]( qint64 timestamp, SystemError::ErrorCode error )
    {
        m_tcpSock->terminateAsyncIO( true );
        {
            QMutexLocker lk( &m_mutex );
            m_tcpSock.reset();
        }
        handlerFunc( timestamp, error );
    };

    using namespace std::placeholders;
    if( !m_tcpSock->connectAsync(
            SocketAddress( HostAddress( QLatin1String( DEFAULT_NIST_SERVER ) ), DAYTIME_PROTOCOL_DEFAULT_PORT ),
            std::bind( &DaytimeNISTFetcher::onConnectionEstablished, this, _1 ) ) )
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_LOG( lit( "NIST time_sync. Failed to start async connect (2) from %1:%2. %3" ).
            arg( QLatin1String( DEFAULT_NIST_SERVER ) ).arg( DAYTIME_PROTOCOL_DEFAULT_PORT ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG2 );
        m_handlerFunc = std::function<void( qint64, SystemError::ErrorCode )>();
        m_tcpSock->terminateAsyncIO( true );
        SystemError::setLastErrorCode( errorCode );
        return false;
    }

    return true;
}

//!Converts time from NIST format to millis from epoch (1970-01-01) UTC
/*!
    Format described at http://www.nist.gov/pml/div688/grp40/its.cfm

    \param actsStr \0 - terminated string of format "JJJJJ YR-MO-DA HH:MM:SS TT L H msADV UTC(NIST) OTM"
*/
static qint64 actsTimeToUTCMillis( const QByteArray& timeStr )
{
    quint32 utcTimeSeconds = 0;
    if( timeStr.size() != sizeof(utcTimeSeconds) )
        return -1;
    memcpy( &utcTimeSeconds, timeStr.constData(), sizeof(utcTimeSeconds) );
    utcTimeSeconds = ntohl( utcTimeSeconds );
    utcTimeSeconds -= SECONDS_FROM_1900_01_01_TO_1970_01_01;
    return ((qint64)utcTimeSeconds) * MILLIS_PER_SEC;
}

void DaytimeNISTFetcher::onConnectionEstablished( SystemError::ErrorCode errorCode ) noexcept
{
    NX_LOG( lit( "NIST time_sync. Connection to NIST server %1:%2 completed with following result: %3" ).
        arg( QLatin1String( DEFAULT_NIST_SERVER ) ).arg( DAYTIME_PROTOCOL_DEFAULT_PORT ).arg(SystemError::toString(errorCode)), cl_logDEBUG2 );

    if( errorCode )
    {
        m_handlerFunc( -1, errorCode );
        return;
    }

    m_timeStr.reserve( MAX_TIME_STR_LENGTH );
    m_timeStr.resize( 0 );

    using namespace std::placeholders;
    if( !m_tcpSock->readSomeAsync( &m_timeStr, std::bind( &DaytimeNISTFetcher::onSomeBytesRead, this, _1, _2 ) ) )
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_LOG( lit( "NIST time_sync. Failed to start async read (1) from %1:%2. %3" ).
            arg( QLatin1String( DEFAULT_NIST_SERVER ) ).arg( DAYTIME_PROTOCOL_DEFAULT_PORT ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG2 );
        m_handlerFunc( -1, errorCode );
    }
}

void DaytimeNISTFetcher::onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead ) noexcept
{
    //TODO #ak take into account rtt/2

    if( errorCode )
    {
        NX_LOG( lit( "NIST time_sync. Failed to read from NIST server %1:%2. %3" ).
            arg( QLatin1String( DEFAULT_NIST_SERVER ) ).arg( DAYTIME_PROTOCOL_DEFAULT_PORT ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG1 );

        if( errorCode == SystemError::timedOut && m_timeStr.size() > 0 )
        {
            //no response during the allotted time period
            //processing what we have
            assert( m_timeStr.size() < m_timeStr.capacity() );
            if( m_timeStr.isEmpty() )
            {
                m_handlerFunc( -1, SystemError::notConnected );
                return;
            }
            m_handlerFunc( actsTimeToUTCMillis( m_timeStr ), SystemError::noError );
            return;
        }
        m_handlerFunc( -1, errorCode );
        return;
    }

    if( bytesRead == 0 )
    {
        NX_LOG( lit( "NIST time_sync. Connection to NIST server %1:%2 closed. Read text %3" ).
            arg( QLatin1String( DEFAULT_NIST_SERVER ) ).arg( DAYTIME_PROTOCOL_DEFAULT_PORT ).arg( QLatin1String(m_timeStr.trimmed()) ), cl_logDEBUG2 );

        //connection closed, analyzing what we have
        assert( m_timeStr.size() < m_timeStr.capacity() );
        if( m_timeStr.isEmpty() )
        {
            m_handlerFunc( -1, SystemError::notConnected );
            return;
        }
        m_handlerFunc( actsTimeToUTCMillis( m_timeStr ), SystemError::noError );
        return;
    }
    else
    {
        NX_LOG( lit( "NIST time_sync. Read %1 bytes from NIST server %2:%3" ).
            arg( bytesRead ).arg( QLatin1String( DEFAULT_NIST_SERVER ) ).arg( DAYTIME_PROTOCOL_DEFAULT_PORT ), cl_logDEBUG2 );
    }

    if( m_timeStr.size() == m_timeStr.capacity() )
    {
        NX_LOG( lit( "NIST time_sync. Read maximum from NIST server %1:%2: %3" ).
            arg( QLatin1String( DEFAULT_NIST_SERVER ) ).arg( DAYTIME_PROTOCOL_DEFAULT_PORT ).arg( QLatin1String( m_timeStr.trimmed() ) ), cl_logDEBUG1 );

        //max data size has been read, ignoring futher data
        m_handlerFunc( actsTimeToUTCMillis( m_timeStr ), SystemError::noError );
        return;
    }

    //reading futher data
    using namespace std::placeholders;
    if( !m_tcpSock->readSomeAsync( &m_timeStr, std::bind( &DaytimeNISTFetcher::onSomeBytesRead, this, _1, _2 ) ) )
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_LOG( lit( "NIST time_sync. Failed to start async read (2) from %1:%2. %3" ).
            arg( QLatin1String( DEFAULT_NIST_SERVER ) ).arg( DAYTIME_PROTOCOL_DEFAULT_PORT ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG2 );
        m_handlerFunc( -1, errorCode );
    }
}
