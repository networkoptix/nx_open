#include "mdns_listener.h"
#include "utils/network/nettools.h"
#include "utils/network/mdns.h"

#ifndef Q_OS_WIN
#include <netinet/in.h>
#include <sys/socket.h>
#endif

static quint16 MDNS_PORT = 5353;
static const int UPDATE_IF_LIST_INTERVAL = 1000 * 60;
static QHostAddress groupAddress(QLatin1String("224.0.0.251"));
static QString groupAddressStr(groupAddress.toString());

// These functions added temporary as in Qt 4.8 they are already in QUdpSocket
bool multicastJoinGroup(UDPSocket* udpSocket, QHostAddress groupAddress, QHostAddress localAddress)
{
    struct ip_mreq imr;

    memset(&imr, 0, sizeof(imr));

    imr.imr_multiaddr.s_addr = htonl(groupAddress.toIPv4Address());
    imr.imr_interface.s_addr = htonl(localAddress.toIPv4Address());

    int res = setsockopt(udpSocket->handle(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&imr, sizeof(struct ip_mreq));
    if (res == -1)
        return false;

    return true;
}

bool multicastLeaveGroup(UDPSocket* udpSocket, QHostAddress groupAddress)
{
    struct ip_mreq imr;

    imr.imr_multiaddr.s_addr = htonl(groupAddress.toIPv4Address());
    imr.imr_interface.s_addr = INADDR_ANY;

    int res = setsockopt(udpSocket->handle(), IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&imr, sizeof(struct ip_mreq));
    if (res == -1)
        return false;

    return true;
}

// -------------- QnMdnsListener ------------

QnMdnsListener::QnMdnsListener()
{
    updateSocketList();
    readDataFromSocket();
}

QnMdnsListener::~QnMdnsListener()
{
    deleteSocketList();
}

void QnMdnsListener::registerConsumer(long handle)
{
    m_data.insert(handle, ConsumerDataList());
}

QnMdnsListener::ConsumerDataList QnMdnsListener::getData(long handle)
{
    ConsumersMap::iterator itr = m_data.find(handle);
    if (itr == m_data.end())
        return QnMdnsListener::ConsumerDataList();
    if (itr == m_data.begin())
        readDataFromSocket();
    ConsumerDataList rez = *itr;
    itr.value().clear();
    return rez;
}

void QnMdnsListener::readDataFromSocket()
{
    quint8 tmpBuffer[1024*16];

    if (m_socketLifeTime.elapsed() > UPDATE_IF_LIST_INTERVAL)
        updateSocketList();

    for (int i = 0; i < m_socketList.size(); ++i)
    {
        UDPSocket* sock = m_socketList[i];
        while (sock->hasData())
        {
            QString removeAddress;
            quint16 removePort;
            int datagramSize = sock->recvFrom(tmpBuffer, sizeof(tmpBuffer), removeAddress, removePort);
            if (datagramSize > 0) {
                QByteArray responseData((const char*) tmpBuffer, datagramSize);
                QString localAddress = sock->getLocalAddress();
                    if (m_localAddressList.contains(removeAddress))
                        continue; // ignore own packets
                for (ConsumersMap::iterator itr = m_data.begin(); itr != m_data.end(); ++itr) {
                    itr.value().append(ConsumerData(responseData, localAddress, removeAddress));
                }
            }
        }

        
        // send request for next read

        MDNSPacket request;
        request.addQuery();
        QByteArray datagram;
        request.toDatagram(datagram);
        sock->sendTo(datagram.data(), datagram.size(), groupAddressStr, MDNS_PORT);
    }
}

void QnMdnsListener::updateSocketList()
{
    deleteSocketList();
    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        UDPSocket* socket = new UDPSocket();
        if (socket->setLocalAddressAndPort(iface.address.toString(), MDNS_PORT) && multicastJoinGroup(socket, groupAddress, iface.address))
        {
            m_socketList << socket;
            m_localAddressList << socket->getLocalAddress();
        }
        else {
            delete socket;
        }
    }
    m_socketLifeTime.restart();
}

void QnMdnsListener::deleteSocketList()
{
    for (int i = 0; i < m_socketList.size(); ++i)
    {
        multicastLeaveGroup(m_socketList[i], groupAddress);
        delete m_socketList[i];
    }
    m_socketList.clear();
    m_localAddressList.clear();
}

QStringList QnMdnsListener::getLocalAddressList() const
{
    return m_localAddressList;
}



Q_GLOBAL_STATIC(QnMdnsListener, inst);

QnMdnsListener* QnMdnsListener::instance()
{
    return inst();
}
