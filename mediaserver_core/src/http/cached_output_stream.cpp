/**********************************************************
* 16 nov 2012
* a.kolesnikov
***********************************************************/

#include "cached_output_stream.h"

#include <utils/network/tcp_connection_processor.h>
#include <utils/network/socket.h>


CachedOutputStream::CachedOutputStream( QnTCPConnectionProcessor* const tcpOutput )
:
    m_tcpOutput( tcpOutput ),
    m_failed( false )
{
    setObjectName( "CachedOutputStream" );
}

CachedOutputStream::~CachedOutputStream()
{
    stop();
}

void CachedOutputStream::postPacket( const QByteArray& data )
{
    if( data.isEmpty() )
        return;
    m_packetsToSend.push( data );
}

bool CachedOutputStream::failed() const
{
    return m_failed;
}

size_t CachedOutputStream::packetsInQueue() const
{
    return m_packetsToSend.size();
}

void CachedOutputStream::pleaseStop()
{
    m_packetsToSend.push( QByteArray() );   //signaling thread that it is time to die
    QnLongRunnable::pleaseStop();
}

void CachedOutputStream::run()
{
    initSystemThreadId();
    while( !needToStop() )
    {
        QByteArray packetToSend;
        m_packetsToSend.pop( packetToSend );
        if( packetToSend.isEmpty() )
            break;

        if( !m_tcpOutput->sendBuffer( packetToSend ) )
        {
            m_failed = true;
            break;
        }
    }
}
