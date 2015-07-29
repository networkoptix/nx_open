
#include "servers_finder.h"

#include <functional>

#include <QTimer>
#include <QElapsedTimer>
#include <QUdpSocket>

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkInterface>
#include <QNetworkAccessManager>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <base/server_info.h>
#include <base/requests.h>

#ifdef WIN32
    #include <WinSock2.h>
    #include <WS2tcpip.h>
#else
    #error Not supported target os
#endif

namespace
{
    const QString kMediaServerType = "Media Server";
    
    const QString kApplicationTypeTag = "application";
    const QString kTypeTag = "type";
    const QString kReplyTag = "reply";
      
    enum 
    {
        kUpdateServersInfoInterval = 2000
        , kHostLifeTimeMs = kUpdateServersInfoInterval * 2
        , kUnknownHostTimeout = 60 * 1000
        , kServerAliveTimeout = 12 * 1000
    };

    enum { kInvalidOpSize = -1 };

    /// Setupsmulticast parameters. Aviods error in QUdpSocket
    bool setupMulticast(QAbstractSocket *socket
        , const QString &interfaceAddress
        , const QString &multicastAddress)
    {
        if (!socket || interfaceAddress.isEmpty() || multicastAddress.isEmpty()
            || (socket->state() != QAbstractSocket::BoundState))
        {
            return false;
        }

        ip_mreq request;
        request.imr_multiaddr.s_addr = inet_addr(multicastAddress.toLatin1());
        request.imr_interface.s_addr = inet_addr(interfaceAddress.toLatin1());

        enum { kNoError = 0 };
        const auto handle = socket->socketDescriptor();
        return (setsockopt( handle, IPPROTO_IP, IP_ADD_MEMBERSHIP
            , reinterpret_cast<const char*>(&request), sizeof(request)) == kNoError);
    }

    
    bool isDifferentFlags(const rtu::BaseServerInfo &first
        , const rtu::BaseServerInfo &second
        , rtu::Constants::ServerFlags excludeFlags)
    {
        const rtu::Constants::ServerFlags firstFlags = (first.flags & ~excludeFlags);
        const rtu::Constants::ServerFlags secondFlags = (second.flags & ~excludeFlags);
        return (firstFlags != secondFlags);
    }

}

namespace
{
    const QHostAddress kMulticastGroupAddress = QHostAddress("239.255.11.11");
    const quint16 kMulticastGroupPort = 5007;
    
    const QByteArray kSearchRequestBody("{ magic: \"7B938F06-ACF1-45f0-8303-98AA8057739A\" }");
    const int kSearchRequestSize = kSearchRequestBody.size();
}

namespace 
{
    bool parseUdpPacket(const QByteArray &data
        , rtu::BaseServerInfo &info)
    {
        const QJsonObject jsonResponseObj = QJsonDocument::fromJson(data.data()).object();
        const auto &itReply = jsonResponseObj.find(kReplyTag);
        const QJsonObject &jsonObject = (itReply != jsonResponseObj.end() 
            ? itReply.value().toObject() : jsonResponseObj);
        
        const auto &itAppType = jsonObject.find(kApplicationTypeTag);
        const bool isCorrectApp = 
            (itAppType != jsonObject.end() && itAppType.value().toString() == kMediaServerType);
        const auto &itNativeType = jsonObject.find(kTypeTag);
        const bool isCorrectNative = 
            (itNativeType != jsonObject.end() && itNativeType.value().toString() == kMediaServerType);
        if (!isCorrectApp && !isCorrectNative)
            return false;

       // if (!jsonObject.value("systemName").toString().contains("nx1_"))
       //     return false;
        
        rtu::parseModuleInformationReply(jsonObject, info);

        return true;
    }

    struct ServerInfoData
    {
        qint64 timestamp;
        qint64 visibleAddressTimestamp;
        rtu::BaseServerInfo info;

        ServerInfoData(qint64 newTimestamp
            , rtu::BaseServerInfo newInfo);
    };

    ServerInfoData::ServerInfoData(qint64 newTimestamp
        , rtu::BaseServerInfo newInfo)
        : timestamp(newTimestamp)
        , visibleAddressTimestamp(- newTimestamp * 2) /// for correct first initialization 
        , info(newInfo)
    {
    }

}

///

class rtu::ServersFinder::Impl : public QObject
{
public:
    Impl(rtu::ServersFinder *owner);
    
    virtual ~Impl();

public:
    void waitForServer(const QUuid &id);

private:
    typedef QSharedPointer<QUdpSocket> SocketPtr;

    void checkOutdatedServers();

    typedef QHash<QString, qint64> Hosts;
    /// Returns list with removed addresses
    QStringList checkOutdatedHosts(Hosts &targetHostsList
        , int lifetime);

    void updateServers();
    
    /// Reads answers to sent "magic" messages
    void readResponseData(const SocketPtr &socket);

    bool readResponsePacket(const SocketPtr &socket);

    /// Reads incomming "magic" packets
    void readRequestData();

    bool readRequestPacket();

    /// Returns true if it is entity with new address, otherwise false
    bool onEntityDiscovered(const QString &address
        , Hosts &targetHostsList);

    bool readPendingPacket(const SocketPtr &socket
        , QByteArray &data
        , QHostAddress *sender = nullptr
        , quint16 *port = nullptr);

private:
    typedef QHash<QHostAddress, SocketPtr> SocketsContainer;
    typedef QHash<QUuid, ServerInfoData> ServerDataContainer;

    rtu::ServersFinder *m_owner;
    SocketsContainer m_sockets;
    ServerDataContainer m_infos;
    rtu::IDsVector m_waitedServers;
    QTimer m_timer;
    QElapsedTimer m_msecCounter;
    SocketPtr m_serverSocket;
    Hosts m_knownHosts;
    Hosts m_unknownHosts;
};

rtu::ServersFinder::Impl::Impl(rtu::ServersFinder *owner)
    : QObject(owner)
    , m_owner(owner)
    , m_sockets()
    , m_infos()
    , m_timer()
    , m_msecCounter()
    , m_serverSocket()
    , m_knownHosts()
    , m_unknownHosts()
{
    QObject::connect(&m_timer, &QTimer::timeout, this, &Impl::updateServers);
    m_timer.setInterval(kUpdateServersInfoInterval);
    m_timer.start();

    m_msecCounter.start();
}

rtu::ServersFinder::Impl::~Impl() 
{
    m_timer.stop();
    for(auto &socket: m_sockets)
    {
        socket->close();
    }
}

void rtu::ServersFinder::Impl::checkOutdatedServers()
{
    const qint64 currentTime = m_msecCounter.elapsed();
    
    IDsVector forRemove;
    for(const auto &infoData: m_infos)
    {
        const qint64 lastUpdateTime = infoData.timestamp;
        const BaseServerInfo &info = infoData.info;
        if ((currentTime - lastUpdateTime) > kServerAliveTimeout)
            forRemove.push_back(info.id);
    }
    
    if (!forRemove.empty())
    {
        for(const auto &id: forRemove)
            m_infos.remove(id);
        
        emit m_owner->serversRemoved(forRemove);
    }
}

QStringList rtu::ServersFinder::Impl::checkOutdatedHosts(Hosts &targetHostsList
    , int lifetime)
{
    QStringList removedHosts;
    const qint64 current = m_msecCounter.elapsed();
    for (auto it = targetHostsList.begin(); it != targetHostsList.end();)
    {
        if ((current - it.value()) > lifetime)
        {
            removedHosts.push_back(it.key());
            it = targetHostsList.erase(it);
            continue;
        }
        ++it;
    }
    return removedHosts;
}

void rtu::ServersFinder::Impl::updateServers()
{
    checkOutdatedServers();
    
    checkOutdatedHosts(m_knownHosts, kHostLifeTimeMs);
    const QStringList removedUnknownHosts =
        checkOutdatedHosts(m_unknownHosts, kUnknownHostTimeout);
    for (const QString &address: removedUnknownHosts)
    {
        emit m_owner->unknownRemoved(address);
    }

    if (!m_serverSocket)
    {
        m_serverSocket.reset(new QUdpSocket());
        if (!m_serverSocket->bind(QHostAddress::AnyIPv4, kMulticastGroupPort, QAbstractSocket::ReuseAddressHint))
            m_serverSocket.reset();

        QObject::connect(m_serverSocket.data(), &QUdpSocket::readyRead, this, [this]
        {
            readRequestData();
        });

    }

    readRequestData();

    const auto allAddresses = QNetworkInterface::allAddresses();
    for(const auto &address: allAddresses)
    {
        if ((address.protocol() != QAbstractSocket::IPv4Protocol))
            continue;

        SocketsContainer::iterator &it = m_sockets.find(address);
        if (it == m_sockets.end())
        {
            it = m_sockets.insert(address, SocketPtr(new QUdpSocket()));
            QObject::connect(it->data(), &QUdpSocket::readyRead, this, [this, it]
            {
                readResponseData(*it);
            });

            if (m_serverSocket)
            {
                /// join to multicast group on this address
                setupMulticast(m_serverSocket.data(), address.toString()
                    , kMulticastGroupAddress.toString());
            }
        }

        const SocketPtr &socket = *it;
        if (socket->state() != QAbstractSocket::BoundState)
        {
            socket->close();
            if (!socket->bind(address))
                continue;
        }

        readResponseData(socket);

        int written = 0;
        while(written < kSearchRequestBody.size())
        {
            const int currentWritten = socket->writeDatagram(kSearchRequestBody.data() + written
                , kSearchRequestSize - written, kMulticastGroupAddress, kMulticastGroupPort);

            if (currentWritten == kInvalidOpSize)
                break;

            written += currentWritten;
        }

        if (written != kSearchRequestSize)
            socket->close();
    }
}

void rtu::ServersFinder::Impl::readResponseData(const SocketPtr &socket)
{
    while(socket->hasPendingDatagrams() && readResponsePacket(socket)) {}
}

void rtu::ServersFinder::Impl::readRequestData()
{
    while(m_serverSocket->hasPendingDatagrams() && readRequestPacket()) {}
}

bool rtu::ServersFinder::Impl::onEntityDiscovered(const QString &address
    , Hosts &targetHostsList)
{
    const auto it = targetHostsList.find(address);
    const qint64 timestamp = m_msecCounter.elapsed();
    if (it == targetHostsList.end())
    {
        targetHostsList.insert(address, timestamp);
        return true;
    }

    it.value() = timestamp;
    return false;
}

bool rtu::ServersFinder::Impl::readRequestPacket()
{
    QByteArray data;
    QHostAddress sender;
    quint16 port = 0;
    if (!readPendingPacket(m_serverSocket, data, &sender, &port))
        return false;

    for (const auto socket: m_sockets)
    {
        if ((socket->localAddress() == sender) && (socket->localPort() == port))
        {
            /// This packet from current instance of tool
            return true;
        }
    }

    if (!data.contains(kSearchRequestBody))
        return true;

    const QString &address = sender.toString();
    const bool isKnown = (m_knownHosts.find(address) != m_knownHosts.end());
    if (!isKnown && onEntityDiscovered(address, m_unknownHosts))
        emit m_owner->unknownAdded(address);

    return true;
}

bool rtu::ServersFinder::Impl::readPendingPacket(const SocketPtr &socket
    , QByteArray &data
    , QHostAddress *sender
    , quint16 *port)
{
    const int pendingDataSize = socket->pendingDatagramSize();
    
    enum { kZeroSymbol = 0 };
    data.resize(pendingDataSize);
    data.fill(kZeroSymbol);

    int readBytes = 0;
    while ((socket->state() == QAbstractSocket::BoundState) && (readBytes < pendingDataSize))
    {
        const int read = socket->readDatagram(data.data() + readBytes
            , pendingDataSize - readBytes, sender, port);

        if (read == kInvalidOpSize)
            return false;

        readBytes += read;
    }

    if (readBytes != pendingDataSize)
    {
        socket->close();
        return false;
    }
    return true;
}

bool rtu::ServersFinder::Impl::readResponsePacket(const rtu::ServersFinder::Impl::SocketPtr &socket)
{
    QByteArray data;
    QHostAddress sender;
    quint16 port = 0;
    if (!readPendingPacket(socket, data, &sender, &port))
        return false;

    BaseServerInfo info;
    if (!parseUdpPacket(data, info) )
        return true;

    const QString &host = sender.toString();
    info.hostAddress = host;

    onEntityDiscovered(host, m_knownHosts);

    emit m_owner->serverDiscovered(info);

    const auto it = m_infos.find(info.id);
    const qint64 timestamp = m_msecCounter.elapsed();

    if (it == m_infos.end()) /// If new server multicast arrived
    {
        m_infos.insert(info.id, ServerInfoData(timestamp, info));
        emit m_owner->serverAdded(info);
    }
    else
    {
        it->timestamp = timestamp; /// Update alive info
        BaseServerInfo &oldInfo = it->info;

        bool displayAddressOutdated = false;
        info.displayAddress = oldInfo.displayAddress;
        if (oldInfo.displayAddress == host)
        {
            it->visibleAddressTimestamp = timestamp;
        }
        else if ((timestamp - it->visibleAddressTimestamp) > kHostLifeTimeMs)
        {
            it->visibleAddressTimestamp = timestamp;
            info.displayAddress = host;
            displayAddressOutdated = true;
        }

        if ((info != it->info) || displayAddressOutdated
            || isDifferentFlags(info, it->info, Constants::IsFactoryFlag))
        {
            oldInfo.name = info.name;
            oldInfo.systemName = info.systemName;
            oldInfo.port = info.port;
            oldInfo.displayAddress = info.displayAddress;

            emit m_owner->serverChanged(info);
        }
    }

    const auto itWaited = std::find(m_waitedServers.begin(), m_waitedServers.end(), info.id);
    if (itWaited == m_waitedServers.end())
        return true;

    emit m_owner->serverAppeared(*itWaited);
    m_waitedServers.erase(itWaited);
    return true;
}

void rtu::ServersFinder::Impl::waitForServer(const QUuid &id)
{
    const auto &it = std::find(m_waitedServers.begin(), m_waitedServers.end(), id);
    if (it == m_waitedServers.end())
    {
        m_waitedServers.push_back(id);
        return;
    }

    *it = id;
}

///

rtu::ServersFinder::Holder rtu::ServersFinder::create()
{
    return Holder(new ServersFinder());
}

rtu::ServersFinder::ServersFinder()
    : m_impl(new Impl(this))
{
}

rtu::ServersFinder::~ServersFinder()
{
}

void rtu::ServersFinder::waitForServer(const QUuid &id)
{
    m_impl->waitForServer(id);
}
