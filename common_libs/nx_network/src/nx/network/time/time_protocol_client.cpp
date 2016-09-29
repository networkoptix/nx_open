/**********************************************************
* 10 jul 2014
* a.kolesnikov
***********************************************************/

#include "time_protocol_client.h"

#include <sys/timeb.h>
#include <sys/types.h>

#include <nx/utils/thread/mutex.h>

#include <nx/utils/log/log.h>
#include <nx/network/socket_factory.h>
#include "utils/tz/tz.h"


//TODO #ak try multiple servers in case of error or empty string (it happens pretty often)

static const size_t MAX_TIME_STR_LENGTH = sizeof(quint32); 
static const int SOCKET_READ_TIMEOUT = 7000;
static const int MILLIS_PER_SEC = 1000;
//static const int SEC_PER_MIN = 60;

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
        QnMutexLocker lk( &m_mutex );
        if( !m_tcpSock )
            return;
        tcpSock = m_tcpSock;
    }

    tcpSock->pleaseStopSync();
    m_tcpSock.reset();
}

void TimeProtocolClient::getTimeAsync( std::function<void(qint64, SystemError::ErrorCode)> handlerFunc )
{
    NX_LOGX( lit( "rfc868 time_sync. Starting time synchronization with server %1:%2" ).
        arg( m_timeServer ).arg( kTimeProtocolDefaultPort ), cl_logDEBUG2 );

    {
        QnMutexLocker lk( &m_mutex );
        m_tcpSock.reset(
            SocketFactory::createStreamSocket(
                false, SocketFactory::NatTraversalType::nttDisabled ).release() );
    }

    if( !m_tcpSock->setNonBlockingMode( true ) ||
        !m_tcpSock->setRecvTimeout(SOCKET_READ_TIMEOUT) ||
        !m_tcpSock->setSendTimeout(SOCKET_READ_TIMEOUT) )
    {
        m_tcpSock->post(std::bind(
            &TimeProtocolClient::onConnectionEstablished,
            this,
            SystemError::getLastOSErrorCode()));
        return;
    }

    m_handlerFunc = [this, handlerFunc]( qint64 timestamp, SystemError::ErrorCode error )
    {
        m_tcpSock->pleaseStopSync();
        {
            QnMutexLocker lk( &m_mutex );
            m_tcpSock.reset();
        }
        handlerFunc( timestamp, error );
    };

    using namespace std::placeholders;
    m_tcpSock->connectAsync(
        SocketAddress( HostAddress( m_timeServer ), kTimeProtocolDefaultPort ),
        std::bind( &TimeProtocolClient::onConnectionEstablished, this, _1 ) );
}

namespace
{
    //!Converts time from Time protocol format (rfc868) to millis from epoch (1970-01-01) UTC
    qint64 rfc868TimestampToTimeToUTCMillis( const QByteArray& timeStr )
    {
        quint32 utcTimeSeconds = 0;
        if( (size_t)timeStr.size() < sizeof(utcTimeSeconds) )
            return -1;
        memcpy( &utcTimeSeconds, timeStr.constData(), sizeof(utcTimeSeconds) );
        utcTimeSeconds = ntohl( utcTimeSeconds );
        utcTimeSeconds -= SECONDS_FROM_1900_01_01_TO_1970_01_01;
        return ((qint64)utcTimeSeconds) * MILLIS_PER_SEC;
    }
}

void TimeProtocolClient::onConnectionEstablished( SystemError::ErrorCode errorCode )
{
    NX_LOGX( lit( "rfc868 time_sync. Connection to time server %1:%2 completed with following result: %3" ).
        arg( m_timeServer ).arg( kTimeProtocolDefaultPort ).arg(SystemError::toString(errorCode)), cl_logDEBUG2 );

    if( errorCode )
    {
        m_handlerFunc( -1, errorCode );
        return;
    }

    m_timeStr.reserve( MAX_TIME_STR_LENGTH );
    m_timeStr.resize( 0 );

    using namespace std::placeholders;
    m_tcpSock->readSomeAsync(
        &m_timeStr,
        std::bind( &TimeProtocolClient::onSomeBytesRead, this, _1, _2 ) );
}

void TimeProtocolClient::onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead )
{
    //TODO #ak take into account rtt/2

    if( errorCode )
    {
        NX_LOGX( lit( "rfc868 time_sync. Failed to read from time server %1:%2. %3" ).
            arg( m_timeServer ).arg( kTimeProtocolDefaultPort ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG1 );

        m_handlerFunc( -1, errorCode );
        return;
    }

    if( bytesRead == 0 )
    {
        NX_LOGX( lit( "rfc868 time_sync. Connection to time server %1:%2 closed. Read text %3" ).
            arg( m_timeServer ).arg( kTimeProtocolDefaultPort ).arg( QLatin1String(m_timeStr.trimmed()) ), cl_logDEBUG2 );

        //connection closed
        m_handlerFunc( -1, SystemError::notConnected );
        return;
    }

    NX_LOGX( lit( "rfc868 time_sync. Read %1 bytes from time server %2:%3" ).
        arg( bytesRead ).arg( m_timeServer ).arg( kTimeProtocolDefaultPort ), cl_logDEBUG2 );

    if( m_timeStr.size() >= m_timeStr.capacity() )
    {
        NX_LOGX( lit( "rfc868 time_sync. Read %1 from time server %2:%3" ).
            arg( QLatin1String(m_timeStr.toHex()) ).arg(m_timeServer).arg(kTimeProtocolDefaultPort), cl_logDEBUG1 );

        //max data size has been read, ignoring futher data
        m_handlerFunc( rfc868TimestampToTimeToUTCMillis( m_timeStr ), SystemError::noError );
        return;
    }

    //reading futher data
    using namespace std::placeholders;
    m_tcpSock->readSomeAsync(
        &m_timeStr,
        std::bind( &TimeProtocolClient::onSomeBytesRead, this, _1, _2 ) );
}
