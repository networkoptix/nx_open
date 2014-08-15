/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#include "networkoptixmodulerevealer.h"

#include <memory>

#include <QtCore/QDateTime>

#include "socket.h"
#include "system_socket.h"
#include "../common/log.h"
#include "../common/systemerror.h"


NetworkOptixModuleRevealer::NetworkOptixModuleRevealer(
    const QString& moduleType,
    const QString& moduleVersion,
    const TypeSpecificParamMap& moduleSpecificParameters,
    const QHostAddress& multicastGroupAddress,
    const unsigned int multicastGroupPort )
{
    ::srand( ::time(NULL) );

    m_revealResponse.type = moduleType;
    m_revealResponse.version = moduleVersion;
    m_revealResponse.typeSpecificParameters = moduleSpecificParameters;
    //generating random seed
    m_revealResponse.seed = QUuid::createUuid();

    const QList<QHostAddress>& interfaceAddresses = QNetworkInterface::allAddresses();
    for( QList<QHostAddress>::const_iterator
        addrIter = interfaceAddresses.begin();
        addrIter != interfaceAddresses.end();
        ++addrIter )
    {
        if( addrIter->protocol() != QAbstractSocket::IPv4Protocol )
            continue;

        const QHostAddress& localAddressToUse = *addrIter; //using any address of interface
        try
        {
            //if( localAddressToUse == QHostAddress(lit("127.0.0.1")) )
            //    continue;
            std::auto_ptr<UDPSocket> sock( new UDPSocket() );
            if( !sock->bind( SocketAddress(localAddressToUse.toString(), 0) ) ||
                !sock->setReuseAddrFlag(true) ||
                !sock->bind( SocketAddress(localAddressToUse.toString(), multicastGroupPort) ) ||
                !sock->joinGroup( multicastGroupAddress.toString(), localAddressToUse.toString() ) )
            {
                SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                NX_LOG( lit("NetworkOptixModuleRevealer. Failed to bind to local address %1:%2 and join multicast group %3. %4").
                    arg(localAddressToUse.toString()).arg(multicastGroupPort).arg(multicastGroupAddress.toString()).arg(SystemError::toString(prevErrorCode)), cl_logERROR );
                continue;
            }
            m_sockets.push_back( sock.release() );
        }
        catch( const std::exception& e )
        {
            NX_LOG( lit("NetworkOptixModuleRevealer. Failed to create socket on local address %1. %2").arg(localAddressToUse.toString()).arg(QString::fromLatin1(e.what())), cl_logERROR );
        }
    }
}

NetworkOptixModuleRevealer::~NetworkOptixModuleRevealer()
{
    stop();
    for( std::vector<AbstractDatagramSocket*>::size_type
        i = 0;
        i < m_sockets.size();
        ++i )
    {
        delete m_sockets[i];
    }
    m_sockets.clear();
}

void NetworkOptixModuleRevealer::pleaseStop()
{
    m_pollSet.interrupt();
    QnLongRunnable::pleaseStop();
}

static const unsigned int errorWaitTimeoutMs = 1000;
static const unsigned int MULTICAST_GROUP_JOIN_TIMEOUT_MS = 60000;

void NetworkOptixModuleRevealer::run()
{
    initSystemThreadId();
    NX_LOG( lit("NetworkOptixModuleRevealer started"), cl_logDEBUG1 );

    static const unsigned int REVEAL_PACKET_RESPONSE_LENGTH = 256;
    quint8 revealPacketResponse[REVEAL_PACKET_RESPONSE_LENGTH];
    //serializing reveal response
    quint8* revealResponseBufStart = revealPacketResponse;
    if( !m_revealResponse.serialize( &revealResponseBufStart, revealPacketResponse + sizeof(revealPacketResponse) ) )
    {
        Q_ASSERT( false );
    }

    for( std::vector<AbstractDatagramSocket*>::const_iterator
        it = m_sockets.begin();
        it != m_sockets.end();
        ++it )
    {
        if( !m_pollSet.add( *it, aio::etRead ) )
        {
            Q_ASSERT( false );
        }
    }

    static const size_t READ_BUFFER_SIZE = UDPSocket::MAX_PACKET_SIZE;
    QScopedArrayPointer<quint8> readBuffer( new quint8[UDPSocket::MAX_PACKET_SIZE] );

    while( !needToStop() )
    {
        //TODO/IMPL do join periodically

        int socketCount = m_pollSet.poll( aio::PollSet::INFINITE_TIMEOUT );
        if( socketCount == 0 )
            continue;    //timeout
        if( socketCount < 0 )
        {
            const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
            NX_LOG( lit("NetworkOptixModuleRevealer. poll failed. ").arg(SystemError::toString(prevErrorCode)), cl_logERROR );
            msleep( errorWaitTimeoutMs );
            continue;
        }

        //some socket(s) changed state
        for( aio::PollSet::const_iterator
            it = m_pollSet.begin();
            it != m_pollSet.end();
            ++it )
        {
            if( !(it.eventType() & aio::etRead) )
                continue;

            AbstractDatagramSocket* udpSocket = dynamic_cast<AbstractDatagramSocket*>(it.socket());
            Q_ASSERT( udpSocket );

            //reading socket response
            QString remoteAddressStr;
            unsigned short remotePort = 0;
            int bytesRead = udpSocket->recvFrom( readBuffer.data(), READ_BUFFER_SIZE, remoteAddressStr, remotePort );
            if( bytesRead == -1 )
            {
                const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                NX_LOG( lit("NetworkOptixModuleRevealer. Failed to read socket on local address (%1). %2").
                    arg(udpSocket->getLocalAddress().toString()).arg(SystemError::toString(prevErrorCode)), cl_logERROR );
                continue;
            }

            //parsing received response
            RevealRequest request;
            const quint8* requestBufStart = readBuffer.data();
            if( !request.deserialize( &requestBufStart, readBuffer.data() + bytesRead ) )
            {
                //invalid response
                NX_LOG( lit("NetworkOptixModuleRevealer. Received invalid request from (%1:%2) on local address %3").
                    arg(remoteAddressStr).arg(remotePort).arg(udpSocket->getLocalAddress().toString()), cl_logINFO );
                continue;
            }

            NX_LOG( lit("NetworkOptixModuleRevealer. Received valid reveal request from (%1:%2) on local address %3").
                arg(remoteAddressStr).arg(remotePort).arg(udpSocket->getLocalAddress().toString()), cl_logDEBUG1 );

            //received valid request, sending response
            if( !udpSocket->sendTo( revealPacketResponse, revealResponseBufStart-revealPacketResponse, remoteAddressStr, remotePort ) )
            {
                const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                NX_LOG( lit("NetworkOptixModuleRevealer. Failed to send reveal response to (%1:%2). %3").
                    arg(remoteAddressStr).arg(remoteAddressStr).arg(SystemError::toString(prevErrorCode)), cl_logERROR );
                continue;
            }
        }
    }

    NX_LOG( lit("NetworkOptixModuleRevealer stopped"), cl_logDEBUG1 );
}
