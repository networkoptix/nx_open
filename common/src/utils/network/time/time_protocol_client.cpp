/**********************************************************
* 10 jul 2014
* a.kolesnikov
***********************************************************/

#include "time_protocol_client.h"

#include <sys/timeb.h>
#include <sys/types.h>

#include <QtCore/QMutexLocker>

#include "utils/common/log.h"
#include "utils/network/socket_factory.h"
#include "utils/tz/tz.h"


//TODO #ak try multiple servers in case of error or empty string (it happens pretty often)

static const unsigned int SECONDS_FROM_1900_01_01_TO_1970_01_01 = 2208988800L;
static const size_t MAX_TIME_STR_LENGTH = sizeof(quint32); 
static const unsigned short TIME_PROTOCOL_DEFAULT_PORT = 37;     //time protocol
static const int SOCKET_READ_TIMEOUT = 7000;
static const int MILLIS_PER_SEC = 1000;
static const int SEC_PER_MIN = 60;

TimeProtocolClient::TimeProtocolClient( const QString& timeServer )
:
    m_timeServer( timeServer )
{
    m_timeStr.reserve( MAX_TIME_STR_LENGTH );
}

TimeProtocolClient::~TimeProtocolClient()
{
    pleaseStop();
    join();
}

void TimeProtocolClient::pleaseStop()
{
    //TODO #ak
}

void TimeProtocolClient::join()
{
    //assuming that user stopped using object and TimeProtocolClient::getTimeAsync will never be called,
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

bool TimeProtocolClient::getTimeAsync( std::function<void(qint64, SystemError::ErrorCode)> handlerFunc )
{
    NX_LOG( lit( "rfc868 time_sync. Starting time synchronization with server %1:%2" ).
        arg( m_timeServer ).arg( TIME_PROTOCOL_DEFAULT_PORT ), cl_logDEBUG2 );

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
            SocketAddress( HostAddress( m_timeServer ), TIME_PROTOCOL_DEFAULT_PORT ),
            std::bind( &TimeProtocolClient::onConnectionEstablished, this, _1 ) ) )
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_LOG( lit( "rfc868 time_sync. Failed to start async connect (2) from %1:%2. %3" ).
            arg( m_timeServer ).arg( TIME_PROTOCOL_DEFAULT_PORT ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG2 );
        m_handlerFunc = std::function<void( qint64, SystemError::ErrorCode )>();
        m_tcpSock->terminateAsyncIO( true );
        SystemError::setLastErrorCode( errorCode );
        return false;
    }

    return true;
}

namespace
{
    //!Converts time from Time protocol format (rfc868) to millis from epoch (1970-01-01) UTC
    qint64 rfc868TimestampToTimeToUTCMillis( const QByteArray& timeStr )
    {
        quint32 utcTimeSeconds = 0;
        if( timeStr.size() < sizeof(utcTimeSeconds) )
            return -1;
        memcpy( &utcTimeSeconds, timeStr.constData(), sizeof(utcTimeSeconds) );
        utcTimeSeconds = ntohl( utcTimeSeconds );
        utcTimeSeconds -= SECONDS_FROM_1900_01_01_TO_1970_01_01;
        return ((qint64)utcTimeSeconds) * MILLIS_PER_SEC;
    }
}

void TimeProtocolClient::onConnectionEstablished( SystemError::ErrorCode errorCode )
{
    NX_LOG( lit( "rfc868 time_sync. Connection to time server %1:%2 completed with following result: %3" ).
        arg( m_timeServer ).arg( TIME_PROTOCOL_DEFAULT_PORT ).arg(SystemError::toString(errorCode)), cl_logDEBUG2 );

    if( errorCode )
    {
        m_handlerFunc( -1, errorCode );
        return;
    }

    m_timeStr.reserve( MAX_TIME_STR_LENGTH );
    m_timeStr.resize( 0 );

    using namespace std::placeholders;
    if( !m_tcpSock->readSomeAsync( &m_timeStr, std::bind( &TimeProtocolClient::onSomeBytesRead, this, _1, _2 ) ) )
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_LOG( lit( "rfc868 time_sync. Failed to start async read (1) from %1:%2. %3" ).
            arg( m_timeServer ).arg( TIME_PROTOCOL_DEFAULT_PORT ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG2 );
        m_handlerFunc( -1, errorCode );
    }
}

void TimeProtocolClient::onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead )
{
    //TODO #ak take into account rtt/2

    if( errorCode )
    {
        NX_LOG( lit( "rfc868 time_sync. Failed to read from time server %1:%2. %3" ).
            arg( m_timeServer ).arg( TIME_PROTOCOL_DEFAULT_PORT ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG1 );

        m_handlerFunc( -1, errorCode );
        return;
    }

    if( bytesRead == 0 )
    {
        NX_LOG( lit( "rfc868 time_sync. Connection to time server %1:%2 closed. Read text %3" ).
            arg( m_timeServer ).arg( TIME_PROTOCOL_DEFAULT_PORT ).arg( QLatin1String(m_timeStr.trimmed()) ), cl_logDEBUG2 );

        //connection closed
        m_handlerFunc( -1, SystemError::notConnected );
        return;
    }

    NX_LOG( lit( "rfc868 time_sync. Read %1 bytes from time server %2:%3" ).
        arg( bytesRead ).arg( m_timeServer ).arg( TIME_PROTOCOL_DEFAULT_PORT ), cl_logDEBUG2 );

    if( m_timeStr.size() >= m_timeStr.capacity() )
    {
        NX_LOG( lit( "rfc868 time_sync. Read %1 from time server %2:%3" ).
            arg( QLatin1String(m_timeStr.toHex()) ).arg(m_timeServer).arg(TIME_PROTOCOL_DEFAULT_PORT), cl_logDEBUG1 );

        //max data size has been read, ignoring futher data
        m_handlerFunc( rfc868TimestampToTimeToUTCMillis( m_timeStr ), SystemError::noError );
        return;
    }

    //reading futher data
    using namespace std::placeholders;
    if( !m_tcpSock->readSomeAsync( &m_timeStr, std::bind( &TimeProtocolClient::onSomeBytesRead, this, _1, _2 ) ) )
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_LOG( lit( "rfc868 time_sync. Failed to start async read (2) from %1:%2. %3" ).
            arg( m_timeServer ).arg( TIME_PROTOCOL_DEFAULT_PORT ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG2 );
        m_handlerFunc( -1, errorCode );
    }
}
