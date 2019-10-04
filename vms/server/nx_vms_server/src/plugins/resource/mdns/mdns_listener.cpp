#include "mdns_listener.h"

#ifdef ENABLE_MDNS

#include <nx/network/nettools.h>
#include <nx/network/system_socket.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

#ifndef Q_OS_WIN
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "mdns_packet.h"

static quint16 MDNS_PORT = 5353;
static const int UPDATE_IF_LIST_INTERVAL = 1000 * 60;
static QString groupAddress(QLatin1String("224.0.0.251"));
static const int kMaxMdnsPacketCount = 128;


using nx::network::UDPSocket;

// -------------- QnMdnsListener ------------

const std::chrono::seconds QnMdnsListener::kRefreshTimeout(1);

QnMdnsListener::QnMdnsListener()
:
    m_receiveSocket(0),
    m_consumersData(new detail::ConsumerDataList)
{
    updateSocketList();
    readDataFromSocket();
}

QnMdnsListener::~QnMdnsListener()
{
    deleteSocketList();
}

void QnMdnsListener::registerConsumer(std::uintptr_t id)
{
    m_consumersData->registerConsumer(id);
}

std::shared_ptr<const ConsumerData> QnMdnsListener::getData(std::uintptr_t id)
{
    auto data = m_consumersData->data(id);
    NX_ASSERT(
        (bool) data,
        lm("No such consumer %1, did you forget to register?").args((uint64_t) id));

    if (data == nullptr)
        return nullptr;

    if (needRefresh())
    {
        m_consumersData->clearRead();
        readDataFromSocket();
    }
    return data;
}

bool QnMdnsListener::needRefresh() const
{
    return std::chrono::steady_clock::now() - m_lastRefreshTime > kRefreshTimeout;
}

QString QnMdnsListener::getBestLocalAddress(const QString& remoteAddress)
{
    if (m_localAddressList.isEmpty())
        return QString();

    int bestEq = -1;
    int bestIndex = 0;
    for (int i = 0; i < m_localAddressList.size(); ++i)
    {
        const int eq = (int) nx::utils::maxPrefix(
            m_localAddressList[i].toStdString(),
            remoteAddress.toStdString()).size();

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
        nx::network::AbstractDatagramSocket* sock = m_socketList[i];

        // send request for next read
        QnMdnsPacket request;
        request.addQuery();
        QByteArray datagram;
        request.toDatagram(datagram);
        sock->sendTo(datagram.data(), datagram.size(), groupAddress, MDNS_PORT);
    }

    m_lastRefreshTime = std::chrono::steady_clock::now();
}

void QnMdnsListener::readSocketInternal(nx::network::AbstractDatagramSocket* socket, const QString& localAddress)
{
    quint8 tmpBuffer[1024*16];
    while (socket->hasData())
    {
        nx::network::SocketAddress remoteEndpoint;
        int datagramSize = socket->recvFrom(tmpBuffer, sizeof(tmpBuffer), &remoteEndpoint);
        if (datagramSize <= 0)
            continue;

        QByteArray responseData((const char*) tmpBuffer, datagramSize);
        if (m_localAddressList.contains(remoteEndpoint.address.toString()))
            continue; //< ignore own packets

        const auto remoteIp = remoteEndpoint.address.toString();
        const auto localIp = localAddress.isEmpty()
                ? getBestLocalAddress(remoteEndpoint.address.toString())
                : localAddress;

        m_consumersData->addData(remoteIp, localIp, responseData);
        NX_VERBOSE(this, lm("Add data %1 -> %2, size %3").args(
            remoteIp, localIp, responseData.size()));
    }
}

void QnMdnsListener::updateSocketList()
{
    deleteSocketList();
    using namespace nx::network;
    for (const auto& address: allLocalAddresses(AddressFilter::onlyFirstIpV4))
    {
        std::unique_ptr<UDPSocket> sock(new UDPSocket(AF_INET));
        QString localAddress = address.toString();
        if( sock->bind( nx::network::SocketAddress(address.toString())))
        {
            sock->setMulticastIF(localAddress);
            m_socketList << sock.release();
            m_localAddressList << localAddress;
        }
    }

    m_receiveSocket = nx::network::SocketFactory::createDatagramSocket().release();
    m_receiveSocket->setReuseAddrFlag(true);
    m_receiveSocket->bind( nx::network::SocketAddress( nx::network::HostAddress::anyHost, MDNS_PORT ) );

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

#endif // ENABLE_MDNS
