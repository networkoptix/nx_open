/**********************************************************
* 16 nov 2012
* a.kolesnikov
***********************************************************/

#include "cached_output_stream.h"

#include <network/tcp_connection_processor.h>
#include <nx/network/socket.h>


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

void CachedOutputStream::postPacket(const QByteArray& data, int maxQueueSize)
{
    if (data.isEmpty())
        return;
    if (maxQueueSize > -1 && m_packetsToSend.size() > maxQueueSize)
    {
        QnMutexLocker lk(&m_mutex);
        while (m_packetsToSend.size() > maxQueueSize && !m_failed && !needToStop())
            m_cond.wait(lk.mutex());

        if (m_failed || needToStop())
            return;
    }
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
    m_cond.wakeAll();
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
        m_cond.wakeAll();
    }
    m_cond.wakeAll();
}
