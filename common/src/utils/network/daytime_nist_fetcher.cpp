/**********************************************************
* 10 jul 2014
* a.kolesnikov
***********************************************************/

#include "daytime_nist_fetcher.h"

#include <sys/timeb.h>
#include <sys/types.h>

#include "utils/network/socket_factory.h"


static const size_t MAX_TIME_STR_LENGTH = 256; 
static const char* DEFAULT_NIST_SERVER = "time.nist.gov";
static const unsigned short DAYTIME_PROTOCOL_DEFAULT_PORT = 13;
static const int SOCKET_READ_TIMEOUT = 7000;
static const int MILLIS_PER_SEC = 1000;
static const int SEC_PER_MIN = 60;

DaytimeNISTFetcher::DaytimeNISTFetcher()
:
    m_readBufferSize( 0 )
{
    m_timeStr.resize( MAX_TIME_STR_LENGTH );
}

DaytimeNISTFetcher::~DaytimeNISTFetcher()
{
}

void DaytimeNISTFetcher::pleaseStop()
{
    if( !m_tcpSock )
        return;

    aio::AIOService::instance()->removeFromWatch( m_tcpSock, aio::etRead );
    aio::AIOService::instance()->removeFromWatch( m_tcpSock, aio::etWrite );
    m_tcpSock.clear();
}

void DaytimeNISTFetcher::join()
{
}

bool DaytimeNISTFetcher::getTimeAsync( std::function<void(qint64, SystemError::ErrorCode)> handlerFunc )
{
    m_readBufferSize = 0;

    m_tcpSock = QSharedPointer<AbstractStreamSocket>( SocketFactory::createStreamSocket( false, SocketFactory::nttDisabled ) );
    if( !m_tcpSock )
        return false;

    if( !m_tcpSock->setNonBlockingMode( true ) ||
        !m_tcpSock->setRecvTimeout(SOCKET_READ_TIMEOUT) ||
        !aio::AIOService::instance()->watchSocket( m_tcpSock, aio::etWrite, this ) )
    {
        m_tcpSock.clear();
        return false;
    }
    m_handlerFunc = handlerFunc;

    m_tcpSock->connect( QString::fromLatin1(DEFAULT_NIST_SERVER), DAYTIME_PROTOCOL_DEFAULT_PORT, SOCKET_READ_TIMEOUT );
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
    struct timeb tp;
    memset( &tp, 0, sizeof(tp) );
    ftime( &tp );
    return ((qint64)utcTime - tp.timezone * SEC_PER_MIN) * MILLIS_PER_SEC;
}

void DaytimeNISTFetcher::eventTriggered( AbstractSocket* sock, aio::EventType eventType ) throw()
{
    assert( sock == m_tcpSock.data() );

    switch( eventType )
    {
        case aio::etWrite:
            //connection established, waiting for data
            aio::AIOService::instance()->removeFromWatch( m_tcpSock, aio::etWrite );
            if( !aio::AIOService::instance()->watchSocket( m_tcpSock, aio::etRead, this ) )
            {
                m_handlerFunc( -1, SystemError::getLastOSErrorCode() );
                break;
            }
            return;

        case aio::etRead:
        {
            //reading time data
            int readed = m_tcpSock->recv( m_timeStr.data() + m_readBufferSize, m_timeStr.size()-m_readBufferSize );
            if( readed == 0 )
            {
                //connection closed
                assert( m_readBufferSize < m_timeStr.size() );
                m_timeStr[m_readBufferSize] = 0;
                m_handlerFunc( actsTimeToUTCMillis( m_timeStr.constData() ), SystemError::noError );
                break;
            }
            if( readed < 0 )
            {
                const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
                if( errorCode == SystemError::wouldBlock || errorCode == SystemError::again )
                    return; //waiting for more data
                m_handlerFunc( -1, errorCode );
                break;
            }

            m_readBufferSize += readed;
            if( m_readBufferSize == m_timeStr.size() )
            {
                //max data size has been read, ignoring futher data
                m_handlerFunc( actsTimeToUTCMillis( m_timeStr.constData() ), SystemError::noError );
                break;
            }
            return; //waiting futher data
        }

        case aio::etTimedOut:
        {
            //no response during the allotted time period
            if( m_readBufferSize > 0 )
            {
                //processing that we have
                assert( m_readBufferSize < m_timeStr.size() );
                m_timeStr[m_readBufferSize] = 0;
                m_handlerFunc( actsTimeToUTCMillis( m_timeStr.constData() ), SystemError::noError );
            }
            else
            {
                m_handlerFunc( -1, SystemError::timedOut );
            }
            break;
        }

        default:
            m_handlerFunc( -1, SystemError::timedOut ); //TODO get error code
            break;
    }

    aio::AIOService::instance()->removeFromWatch( m_tcpSock, aio::etRead );
    aio::AIOService::instance()->removeFromWatch( m_tcpSock, aio::etWrite );
    m_tcpSock.clear();
}
