/**********************************************************
* 10 jul 2014
* a.kolesnikov
***********************************************************/

#include "daytime_nist_fetcher.h"

#include <sys/timeb.h>
#include <sys/types.h>

#include "utils/common/log.h"
#include "utils/network/socket_factory.h"
#include "utils/tz/tz.h"


//TODO #ak try multiple servers in case of error or empty string (it happens pretty often)

static const size_t MAX_TIME_STR_LENGTH = 256; 
static const char* DEFAULT_NIST_SERVER = "time.nist.gov";
static const unsigned short DAYTIME_PROTOCOL_DEFAULT_PORT = 13;
static const int SOCKET_READ_TIMEOUT = 7000;
static const int MILLIS_PER_SEC = 1000;
static const int SEC_PER_MIN = 60;

DaytimeNISTFetcher::DaytimeNISTFetcher()
{
    m_timeStr.reserve( MAX_TIME_STR_LENGTH );
}

DaytimeNISTFetcher::~DaytimeNISTFetcher()
{
}

void DaytimeNISTFetcher::pleaseStop()
{
    if( !m_tcpSock )
        return;

    m_tcpSock->cancelAsyncIO( aio::etWrite );
    if( m_tcpSock )
        m_tcpSock->cancelAsyncIO( aio::etRead );
    m_tcpSock.reset();
}

void DaytimeNISTFetcher::join()
{
}

bool DaytimeNISTFetcher::getTimeAsync( std::function<void(qint64, SystemError::ErrorCode)> handlerFunc )
{
    NX_LOG( lit( "NIST time_sync. Starting time synchronization with NIST server %1:%2" ).arg( QLatin1String( DEFAULT_NIST_SERVER ) ).arg( DAYTIME_PROTOCOL_DEFAULT_PORT ), cl_logDEBUG2 );

    assert( !m_tcpSock.get() );

    m_tcpSock.reset( SocketFactory::createStreamSocket( false, SocketFactory::nttDisabled ) );
    if( !m_tcpSock )
        return false;

    if( !m_tcpSock->setNonBlockingMode( true ) ||
        !m_tcpSock->setRecvTimeout(SOCKET_READ_TIMEOUT) ||
        !m_tcpSock->setSendTimeout(SOCKET_READ_TIMEOUT) )
    {
        m_tcpSock.reset();
        return false;
    }

    m_handlerFunc = [this, handlerFunc]( qint64 timestamp, SystemError::ErrorCode error )
    {
        m_tcpSock.reset();
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
        m_tcpSock.reset();
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
static qint64 actsTimeToUTCMillis( const char* actsStr )
{
    //skipping separators in the beginning
    while( *actsStr && (*actsStr == '\n' || *actsStr == '\r' || *actsStr == ' ' || *actsStr == '\t') )
        ++actsStr;

    struct tm curTime;
    memset( &curTime, 0, sizeof(curTime) );
    unsigned int mjd = 0, leap = 0, health = 0;
    float msADV = 0;

    sscanf( actsStr, "%d %d-%d-%d %d:%d:%d %d %d %d %f", 
        &mjd, &curTime.tm_year, &curTime.tm_mon, &curTime.tm_mday, 
        &curTime.tm_hour, &curTime.tm_min, &curTime.tm_sec, &curTime.tm_isdst, &leap, &health, &msADV );
    curTime.tm_mon -= 1;    //must be from 0 to 11
    curTime.tm_year += 100; //current year minus 1900
    time_t utcTime = mktime( &curTime );

    if( utcTime < 0 )
        return -1;

    //adjusting to local time: actually we should adjust curTime to local timezone, but it is more simple to do it now with utcTime
    return ((qint64)utcTime + nx_tz::getLocalTimeZoneOffset() * SEC_PER_MIN) * MILLIS_PER_SEC;
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
            m_timeStr.append( '\0' );
            m_handlerFunc( actsTimeToUTCMillis( m_timeStr.constData() ), SystemError::noError );
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
        m_timeStr.append( '\0' );
        m_handlerFunc( actsTimeToUTCMillis( m_timeStr.constData() ), SystemError::noError );
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
        m_tcpSock.reset();
        m_handlerFunc( actsTimeToUTCMillis( m_timeStr.constData() ), SystemError::noError );
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
