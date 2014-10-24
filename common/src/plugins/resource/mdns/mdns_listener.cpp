#include "mdns_listener.h"

#ifdef ENABLE_MDNS

#include <memory>

#include <utils/network/nettools.h>
#include <utils/network/system_socket.h>
#include <utils/network/socket_factory.h>

#ifndef Q_OS_WIN
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "mdns_packet.h"

static quint16 MDNS_PORT = 5353;
static const int UPDATE_IF_LIST_INTERVAL = 1000 * 60;
static QString groupAddress(QLatin1String("224.0.0.251"));


// -------------- QnMdnsListener ------------

static QnMdnsListener* QnMdnsListener_instance = nullptr;

QnMdnsListener::QnMdnsListener()
:
    m_receiveSocket(0)
{
    updateSocketList();
    readDataFromSocket();

    assert(QnMdnsListener_instance == nullptr);
    QnMdnsListener_instance = this;
}

QnMdnsListener::~QnMdnsListener()
{
    QnMdnsListener_instance = nullptr;
    deleteSocketList();
}

void QnMdnsListener::registerConsumer(long handle)
{
    m_data.push_back( std::make_pair( handle, ConsumerDataList() ) );
}

QnMdnsListener::ConsumerDataList QnMdnsListener::getData(long handle)
{
    std::list<std::pair<long, ConsumerDataList> >::iterator itr;
    const std::list<std::pair<long, ConsumerDataList> >::iterator itEnd = m_data.end();
    for( itr = m_data.begin();
        itr != itEnd;
        ++itr )
    {
        if( itr->first == handle )
            break;
    }

    //ConsumersMap::iterator itr = m_data.find(handle);
    if (itr == m_data.end())
        return QnMdnsListener::ConsumerDataList();
    if (itr == m_data.begin())
        readDataFromSocket();
    ConsumerDataList rez = itr->second;
    itr->second.clear();
    return rez;
}

QString QnMdnsListener::getBestLocalAddress(const QString& remoteAddress)
{
    if (m_localAddressList.isEmpty())
        return QString();

    int bestEq = -1;
    int bestIndex = 0;
    for (int i = 0; i < m_localAddressList.size(); ++i)
    {
        int eq = strEqualAmount(m_localAddressList[i].toLatin1().constData(), remoteAddress.toLatin1().constData());
        if (eq > bestEq) {
            bestEq = eq;
            bestIndex = i;
        }
    }
    return m_localAddressList[bestIndex];
}

void QnMdnsListener::readDataFromSocket()
{
    for (int i = 0; i < m_socketList.size(); ++i)
        readSocketInternal(m_socketList[i], m_localAddressList[i]);
    readSocketInternal(m_receiveSocket, QString());

    if (m_socketLifeTime.elapsed() > UPDATE_IF_LIST_INTERVAL)
        updateSocketList();

    for (int i = 0; i < m_socketList.size(); ++i)
    {
        AbstractDatagramSocket* sock = m_socketList[i];
        
        // send request for next read
        QnMdnsPacket request;
        request.addQuery();
        QByteArray datagram;
        request.toDatagram(datagram);
        sock->sendTo(datagram.data(), datagram.size(), groupAddress, MDNS_PORT);
    }
}

void QnMdnsListener::readSocketInternal(AbstractDatagramSocket* socket, QString localAddress)
{
    quint8 tmpBuffer[1024*16];
    while (socket->hasData())
    {
        QString remoteAddress;
        quint16 remotePort;
        int datagramSize = socket->recvFrom(tmpBuffer, sizeof(tmpBuffer), remoteAddress, remotePort);
        if (datagramSize > 0) {
            QByteArray responseData((const char*) tmpBuffer, datagramSize);
            if (m_localAddressList.contains(remoteAddress))
                continue; // ignore own packets
            if (localAddress.isEmpty())
                localAddress = getBestLocalAddress(remoteAddress);

            const std::list<std::pair<long, ConsumerDataList> >::iterator itEnd = m_data.end();
            for( std::list<std::pair<long, ConsumerDataList> >::iterator
                it = m_data.begin();
                it != itEnd;
                ++it )
            {
                it->second.append(ConsumerData(responseData, localAddress, remoteAddress));
            }
        }
    }
}

void QnMdnsListener::updateSocketList()
{
    deleteSocketList();
    for (const QnInterfaceAndAddr& iface: getAllIPv4Interfaces())
    {
        std::unique_ptr<UDPSocket> sock( new UDPSocket() );
        QString localAddress = iface.address.toString();
        //if (socket->bindToInterface(iface))
        if( sock->bind( SocketAddress( iface.address.toString() ) ) )
        {
            sock->setMulticastIF(localAddress);
            m_socketList << sock.release();
            m_localAddressList << localAddress;
        }
    }

    m_receiveSocket = SocketFactory::createDatagramSocket();
    m_receiveSocket->setReuseAddrFlag(true);
    m_receiveSocket->bind( SocketAddress( HostAddress::anyHost, MDNS_PORT ) );

    for (int i = 0; i < m_localAddressList.size(); ++i)
        m_receiveSocket->joinGroup(groupAddress, m_localAddressList[i]);

    m_socketLifeTime.restart();
}

void QnMdnsListener::deleteSocketList()
{
    for (int i = 0; i < m_socketList.size(); ++i)
    {
        delete m_socketList[i];
        if (m_receiveSocket) 
            m_receiveSocket->leaveGroup(groupAddress, m_localAddressList[i]);
    }
    m_socketList.clear();
    m_localAddressList.clear();

    delete m_receiveSocket;
    m_receiveSocket = 0;
}

QStringList QnMdnsListener::getLocalAddressList() const
{
    return m_localAddressList;
}



QnMdnsListener* QnMdnsListener::instance()
{
    return QnMdnsListener_instance;
}

#endif // ENABLE_MDNS
