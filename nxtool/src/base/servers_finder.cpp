
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
#include <QRegularExpression>

#include <nx/mediaserver/api/client.h>

#include <helpers/itf_helpers.h>

#if defined(Q_OS_WIN)
    #include <WinSock2.h>
    #include <WS2tcpip.h>
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACX)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <arpa/inet.h>
#else
    #error Not supported target os
#endif

namespace api = nx::mediaserver::api;

namespace
{
    const QHostAddress kMulticastGroupAddress = QHostAddress("239.255.11.11");
    const quint16 kMulticastGroupPort = 5007;

    const QByteArray kSearchRequestBody("{ magic: \"7B938F06-ACF1-45f0-8303-98AA8057739A\" }");
    const int kSearchRequestSize = kSearchRequestBody.size();
}

namespace
{
    const QString kMediaServerType = "Media Server";

    const QString kApplicationTypeTag = "application";
    const QString kTypeTag = "type";
    const QString kReplyTag = "reply";

    enum
    {
        kUpdateServersInfoInterval = 2000
        , kUnknownHostTimeout = 60 * 1000
        , kServerAliveTimeout = 15 * 1000
        , kMulticastUpdatePeriod = kServerAliveTimeout / 2
    };

    enum { kInvalidOpSize = -1 };

    /// Setups multicast parameters. Aviods error in QUdpSocket
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


    bool isDifferentFlags(const api::BaseServerInfo &first
        , const api::BaseServerInfo &second
        , api::Constants::ServerFlags excludeFlags)
    {
        const api::Constants::ServerFlags firstFlags = (first.flags & ~excludeFlags);
        const api::Constants::ServerFlags secondFlags = (second.flags & ~excludeFlags);
        return (firstFlags != secondFlags);
    }

    typedef bool IsServerPeerType;
    typedef QPair<QUuid, IsServerPeerType> MagicData;

    MagicData extractMulticastData(const QByteArray &data)
    {
        enum { kDelimiterSize = 2 };

        MagicData result = MagicData(QUuid(), false);
        if (!data.contains(kSearchRequestBody) || (data.size() <= kSearchRequestSize))
            return result;

        const QByteArray extraData = data.right(data.size() - (kSearchRequestSize + kDelimiterSize));

        static const char kFieldsDelimiter = ',';
        static const char kValueDelimiter = ':';
        static const QByteArray kSeedTag = "seed";
        static const QByteArray kPeerTypeTag = "peerType";

        const auto fieldsData = extraData.split(kFieldsDelimiter);
        for (QString field: fieldsData)
        {
            const bool isSeed = field.contains(kSeedTag);
            const bool isPeerType = field.contains(kPeerTypeTag);
            if (!isSeed && !isPeerType)
                continue;

            field.remove(QRegularExpression("[{}\"]"));
            const auto value = field.trimmed().split(kValueDelimiter).back();
            if (isSeed)
                result.first = QUuid(value.trimmed());
            else
                result.second = value.contains("PT_Server");
        }

        return result;
    }
}

namespace
{
    bool parseUdpPacket(const QJsonObject &jsonResponseObj
        , api::BaseServerInfo *info)
    {
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

//        if (!jsonObject.value("systemName").toString().contains("000_nx1_"))
//            return false;

        api::Client::parseModuleInformationReply(jsonObject, info);
        return true;
    }

    struct ServerInfoData
    {
        qint64 timestamp;
        qint64 visibleAddressTimestamp;
        api::BaseServerInfo info;

        ServerInfoData(qint64 newTimestamp
            , const api::BaseServerInfo &newInfo);
    };

    ServerInfoData::ServerInfoData(qint64 newTimestamp
        , const api::BaseServerInfo &newInfo)
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

    void updateServerByMulticast(const QUuid &id
        , const QString &address);

    void updateOutdatedByMulticast();

    void checkOutdatedServers();

    typedef QHash<QString, qint64> Hosts;
    /// Returns list with removed addresses
    QStringList checkOutdatedHosts(Hosts &targetHostsList
        , int lifetime);

    void updateServers();

    /// Reads answers to sent "magic" messages
    void readResponseData(const SocketPtr &socket);

    bool processNewServer(api::BaseServerInfo *info
        , const QString &host
        , api::HttpAccessMethod httpAccessMethod);

    /// Reads incoming "magic" packets
    void readMagicData();

    bool readMagicPacket();

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

void rtu::ServersFinder::Impl::updateServerByMulticast(const QUuid &id
    , const QString &address)
{
    const auto successful = [this, address](BaseServerInfo *info)
        { processNewServer(info, address, api::HttpAccessMethod::kUdp); };

    const auto failed = [this, address](const api::Client::ResultCode /* errorCode */
        , api::Client::AffectedEntities /* affectedEntities */)
    {
        if (m_knownHosts.find(address) != m_knownHosts.end())
            return;

        /// if entity
        /// 1) discoverable from current network
        /// 2) but has not added to known servers list up to now
        /// 3) and not available by multicast
        /// - we assume that it is not a server (may be client, for example)
        if (helpers::isDiscoverableFromCurrentNetwork(address))
            return;

        if (onEntityDiscovered(address, m_unknownHosts))
            emit m_owner->unknownAdded(address);
    };

    api::Client::multicastModuleInformation(id, successful, failed, kMulticastUpdatePeriod / 2);
}
void rtu::ServersFinder::Impl::updateOutdatedByMulticast()
{
    const qint64 currentTime = m_msecCounter.elapsed();
    for (ServerDataContainer::const_iterator it = m_infos.begin(); it != m_infos.end(); ++it)
    {
        const auto &data = it.value();
        if ((currentTime - data.timestamp) > kMulticastUpdatePeriod)
            updateServerByMulticast(it.key(), data.info.hostAddress);
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
    updateOutdatedByMulticast();
    checkOutdatedServers();

    checkOutdatedHosts(m_knownHosts, kServerAliveTimeout);
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
            readMagicData();
        });

    }

    readMagicData();

    const auto allAddresses = QNetworkInterface::allAddresses();
    for(const auto &address: allAddresses)
    {
        if ((address.protocol() != QAbstractSocket::IPv4Protocol))
            continue;

        SocketsContainer::iterator it = m_sockets.find(address);
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
    const auto readPacket = [this, socket]() -> bool
    {
        QByteArray data;
        QHostAddress sender;
        quint16 port = 0;
        if (!readPendingPacket(socket, data, &sender, &port))
           return false;

        BaseServerInfo info;
        if (parseUdpPacket(QJsonDocument::fromJson(data.data()).object(), &info))
            processNewServer(&info, sender.toString(), api::HttpAccessMethod::kTcp);

        return true;
    };

    while(socket->hasPendingDatagrams() && readPacket()) {}
}

void rtu::ServersFinder::Impl::readMagicData()
{
    while(m_serverSocket->hasPendingDatagrams() && readMagicPacket()) {}
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

bool rtu::ServersFinder::Impl::readMagicPacket()
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

    /// Assume localhost servers are always visible.
    if (QHostAddress(address) == QHostAddress::LocalHost)
        return true;

    const auto magicData = extractMulticastData(data);
    if (!magicData.first.isNull() && !magicData.second)  /// Extra magic data exist, but peer is not a server.
        return true;                                     /// So, drop this entity. Possibly it is a new client or something else

    const auto &firstTimeProcessor =
        [this, address, magicData](const Callback &secondTimeProcessor)
    {
        const bool isKnown = (m_knownHosts.find(address) != m_knownHosts.end());
        const bool isUnknown = (m_unknownHosts.find(address) != m_unknownHosts.end());
        const bool notKnown = !isKnown && !isUnknown;

        if (notKnown)
        {
            if (secondTimeProcessor)    /// Called at first time
            {
                /// try to add unknown or discover by multicast after period * 3
                /// - to have a chance to discover entity as known or by Http first
                enum { kDelayTimeout = kUpdateServersInfoInterval * 3};
                QTimer::singleShot(kDelayTimeout, secondTimeProcessor);
            }
            else
                updateServerByMulticast(magicData.first, address);
        }
        else if (!isKnown && onEntityDiscovered(address, m_unknownHosts))
            emit m_owner->unknownAdded(address);
    };

    const auto &secondTimeProcessor = [firstTimeProcessor]()
    {
        firstTimeProcessor(Callback());
    };

    firstTimeProcessor(secondTimeProcessor);
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

bool rtu::ServersFinder::Impl::processNewServer(api::BaseServerInfo *info
    , const QString &host
    , api::HttpAccessMethod httpAccessMethod)
{
    info->hostAddress = host;
    info->httpAccessMethod = httpAccessMethod;

    onEntityDiscovered(host, m_knownHosts);
    if (m_unknownHosts.remove(host))
        emit m_owner->unknownRemoved(host);

    const auto it = m_infos.find(info->id);
    const qint64 timestamp = m_msecCounter.elapsed();

    if (it == m_infos.end()) /// If new server multicast arrived
    {
        m_infos.insert(info->id, ServerInfoData(timestamp, *info));
        emit m_owner->serverAdded(*info);
    }
    else if (httpAccessMethod == api::HttpAccessMethod::kTcp //< Gives chance to http to discover server.
        || ((timestamp - it->timestamp) > kMulticastUpdatePeriod)) /// Http access method has priority under multicast

    {
        it->timestamp = timestamp; /// Update alive info
        BaseServerInfo &oldInfo = it->info;

        bool displayAddressOutdated = false;
        info->displayAddress = oldInfo.displayAddress;
        if (oldInfo.displayAddress == host)
        {
            it->visibleAddressTimestamp = timestamp;
        }
        else if ((timestamp - it->visibleAddressTimestamp) > kServerAliveTimeout)
        {
            it->visibleAddressTimestamp = timestamp;
            info->displayAddress = host;
            displayAddressOutdated = true;
        }

        if ((*info != it->info) || displayAddressOutdated
            || isDifferentFlags(*info, it->info, api::Constants::IsFactoryFlag))
        {
            oldInfo.name = info->name;
            oldInfo.systemName = info->systemName;
            oldInfo.port = info->port;
            oldInfo.displayAddress = info->displayAddress;

            emit m_owner->serverChanged(*info);
        }
    }

    emit m_owner->serverDiscovered(*info);

    const auto itWaited = std::find(m_waitedServers.begin(), m_waitedServers.end(), info->id);
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
