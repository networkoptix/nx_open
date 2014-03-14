/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/
#include "networkoptixmodulefinder.h"

#include <memory>

#include <QtCore/QDateTime>
#include <QtCore/QScopedArrayPointer>
#include <QtNetwork/QNetworkInterface>

#include <utils/common/log.h>
#include <utils/common/systemerror.h>
#include <utils/common/product_features.h>

#include "socket.h"
#include "system_socket.h"


#ifndef _WIN32
const unsigned int NetworkOptixModuleFinder::defaultPingTimeoutMillis;
const unsigned int NetworkOptixModuleFinder::defaultKeepAliveMultiply;
#endif

NetworkOptixModuleFinder::NetworkOptixModuleFinder(
    const QHostAddress& multicastGroupAddress,
    const unsigned int multicastGroupPort,
    const unsigned int pingTimeoutMillis,
    const unsigned int keepAliveMultiply )
:
    m_pingTimeoutMillis( pingTimeoutMillis == 0 ? defaultPingTimeoutMillis : pingTimeoutMillis ),
    m_keepAliveMultiply( keepAliveMultiply == 0 ? keepAliveMultiply : defaultKeepAliveMultiply ),
    m_prevPingClock( 0 ),
    m_compatibilityMode(false)
{
    const QList<QHostAddress>& interfaceAddresses = QNetworkInterface::allAddresses();
    for( QList<QHostAddress>::const_iterator
        addrIter = interfaceAddresses.begin();
        addrIter != interfaceAddresses.end();
        ++addrIter )
    {
        if( addrIter->protocol() != QAbstractSocket::IPv4Protocol )
            continue;

        const QHostAddress& addressToUse = *addrIter; //using any address of interface
        try
        {
            //if( addressToUse == QHostAddress(lit("127.0.0.1")) )
            //    continue;
            std::auto_ptr<AbstractDatagramSocket> sock( SocketFactory::createDatagramSocket() );
            sock->bind( addressToUse.toString(), 0 );
            sock->getLocalAddress();    //requesting local address. During this call local port is assigned to socket
            sock->setDestAddr( multicastGroupAddress.toString(), multicastGroupPort );
            m_sockets.push_back( sock.release() );
            m_localNetworkAdresses.insert( addressToUse.toString() );
        }
        catch( const std::exception& e )
        {
            NX_LOG( lit("Failed to create socket on local address %1. %2").arg(addressToUse.toString()).arg(QString::fromLatin1(e.what())), cl_logERROR );
        }
    }
}

NetworkOptixModuleFinder::~NetworkOptixModuleFinder()
{
    stop();
    
    qDeleteAll(m_sockets);
    m_sockets.clear();
}

bool NetworkOptixModuleFinder::isValid() const
{
    return !m_sockets.empty();
}

bool NetworkOptixModuleFinder::isCompatibilityMode() const {
    return m_compatibilityMode;
}

void NetworkOptixModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_compatibilityMode = compatibilityMode;
}

void NetworkOptixModuleFinder::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_pollSet.interrupt();
}

static const unsigned int ERROR_WAIT_TIMEOUT_MS = 1000;

void NetworkOptixModuleFinder::run()
{
    initSystemThreadId();
    NX_LOG( lit("NetworkOptixModuleFinder started"), cl_logDEBUG1 );

    static const unsigned int SEARCH_PACKET_LENGTH = 64;
    quint8 searchPacket[SEARCH_PACKET_LENGTH];
    RevealRequest searchRequest;
    //serializing search packet
    quint8* searchPacketBufStart = searchPacket;
    if( !searchRequest.serialize( &searchPacketBufStart, searchPacket + sizeof(searchPacket) ) )
    {
        Q_ASSERT( false );
    }

    for( std::vector<AbstractDatagramSocket*>::const_iterator
        it = m_sockets.begin();
        it != m_sockets.end();
        ++it )
    {
        if( !m_pollSet.add( *it, PollSet::etRead ) )
        {
            Q_ASSERT( false );
        }
    }

    static const size_t READ_BUFFER_SIZE = UDPSocket::MAX_PACKET_SIZE;
    QScopedArrayPointer<quint8> readBuffer( new quint8[UDPSocket::MAX_PACKET_SIZE] );

    while( !needToStop() )
    {
        quint64 currentClock = QDateTime::currentMSecsSinceEpoch();
        if( currentClock - m_prevPingClock >= m_pingTimeoutMillis )
        {
            //sending request via each socket
            for( std::vector<AbstractDatagramSocket*>::const_iterator
                it = m_sockets.begin();
                it != m_sockets.end();
                ++it )
            {
                if( !(*it)->send( searchPacket, searchPacketBufStart - searchPacket ) )
                {
                    //failed to send packet ???
                    SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                    NX_LOG( lit("NetworkOptixModuleFinder. poll failed. %1").arg(SystemError::toString(prevErrorCode)), cl_logDEBUG1 );
                    //TODO/IMPL if corresponding interface is down, should remove socket from set
                }
            }
            m_prevPingClock = currentClock;
        }

        int socketCount = m_pollSet.poll( m_pingTimeoutMillis - (currentClock - m_prevPingClock) );
        if( socketCount == 0 )
            continue;    //timeout
        if( socketCount < 0 )
        {
            SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
            NX_LOG(lit("NetworkOptixModuleFinder. poll failed. %1").arg(SystemError::toString(prevErrorCode)), cl_logERROR );
            msleep( ERROR_WAIT_TIMEOUT_MS );
            continue;
        }

        currentClock = QDateTime::currentMSecsSinceEpoch();

        //some socket(s) changed state
        for( PollSet::const_iterator
            it = m_pollSet.begin();
            it != m_pollSet.end();
            ++it )
        {
            if( !(it.eventType() & PollSet::etRead) )
                continue;

            AbstractDatagramSocket* udpSocket = dynamic_cast<AbstractDatagramSocket*>(it.socket());
            Q_ASSERT( udpSocket );

            //reading socket response
            QString remoteAddressStr;
            unsigned short remotePort = 0;
            int bytesRead = udpSocket->recvFrom( readBuffer.data(), READ_BUFFER_SIZE, remoteAddressStr, remotePort );
            if( bytesRead == -1 )
            {
                SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                NX_LOG( lit("NetworkOptixModuleFinder. Failed to read socket on local address (%1). %2").
                    arg(udpSocket->getLocalAddress().toString()).arg(SystemError::toString(prevErrorCode)), cl_logERROR );
                continue;
            }

            //parsing recevied response
            RevealResponse response;
            const quint8* responseBufStart = readBuffer.data();
            if( !response.deserialize( &responseBufStart, readBuffer.data() + bytesRead ) )
            {
                //invalid response
                NX_LOG(lit("NetworkOptixModuleFinder. Received invalid response from (%1:%2) on local address %3").
                    arg(remoteAddressStr).arg(remotePort).arg(udpSocket->getLocalAddress().toString()), cl_logDEBUG1 );
                continue;
            }

            if(!m_compatibilityMode && response.customization.toLower() != qnProductFeatures().customizationName.toLower() ) // TODO: #2.1 #Elric #AK check for "default" VS "Vms"
            {
                NX_LOG( lit("NetworkOptixModuleFinder. Ignoring %1 (%2:%3) with different customization %4 on local address %5").
                    arg(response.type).arg(remoteAddressStr).arg(remotePort).arg(response.customization).arg(udpSocket->getLocalAddress().toString()), cl_logDEBUG2 );
                continue;
            }

            //received valid response, checking if already know this enterprise controller
            QHostAddress remoteAddress(remoteAddressStr);
            std::pair<std::map<QString, ModuleContext>::iterator, bool>
                p = m_knownEnterpriseControllers.insert( std::make_pair( response.seed, ModuleContext() ) );
            if( p.second )  //new module
                p.first->second.response = response;
            if( p.first->second.signalledAddresses.insert( remoteAddress.toString() ).second )
            {
                //new enterprise controller found
                const QHostAddress& localAddress = QHostAddress(udpSocket->getLocalAddress().address.toString());
                if( p.second )  //new module found
                {
                    NX_LOG(lit("NetworkOptixModuleFinder. New remote server of type %1 found at address (%2:%3) on local interface %4").
                        arg(response.type).arg(remoteAddressStr).arg(remotePort).arg(localAddress.toString()), cl_logDEBUG1 );
                }
                else    //new address of existing module
                {
                    NX_LOG( lit("NetworkOptixModuleFinder. New address (%2:%3) of remote server of type %1 found on local interface %4").
                        arg(response.type).arg(remoteAddressStr).arg(remotePort).arg(localAddress.toString()), cl_logDEBUG1 );
                }
                emit moduleFound(
                    response.type,
                    response.version,
                    response.typeSpecificParameters,
                    localAddress.toString(),
                    remoteAddress.toString(),
                    m_localNetworkAdresses.find(remoteAddressStr) != m_localNetworkAdresses.end(),
                    response.seed );
            }

            p.first->second.prevResponseReceiveClock = currentClock;
        }

        //checking for expired known hosts...
        for( std::map<QString, ModuleContext>::iterator
            it = m_knownEnterpriseControllers.begin();
            it != m_knownEnterpriseControllers.end();
             )
        {
            if( it->second.prevResponseReceiveClock + m_pingTimeoutMillis*m_keepAliveMultiply > currentClock )
            {
                ++it;
                continue;
            }
            const std::set<QString>::const_iterator& addrIterEnd = it->second.signalledAddresses.end();
            for( std::set<QString>::const_iterator
                addrIter = it->second.signalledAddresses.begin();
                addrIter != addrIterEnd;
                ++addrIter )
            {
                emit moduleLost(
                    it->second.response.type,
                    it->second.response.typeSpecificParameters,
                    *addrIter,
                    m_localNetworkAdresses.find(*addrIter) != m_localNetworkAdresses.end(),
                    it->second.response.seed );
            }
            m_knownEnterpriseControllers.erase( it++ );
        }
    }

    NX_LOG(lit("NetworkOptixModuleFinder stopped"), cl_logDEBUG1 );
}
