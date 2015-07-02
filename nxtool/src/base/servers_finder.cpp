
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

namespace
{
    const QString kMediaServerType = "Media Server";
    
    const QString kApplicationTypeTag = "application";
    const QString kTypeTag = "type";
    const QString kIDTag = "id";
    const QString kSeedTag = "seed";
    const QString kNameTag = "name";
    const QString kSystemNameTag = "systemName";
    const QString kPortTag = "port";
    const QString kRemoteAddressesTag = "remoteAddresses";
    const QString kReplyTag = "reply";
    const QString kFlagsTag = "flags";
      
    enum 
    {
        kUpdateServersInfoInterval = 1000
        , kServerAliveTimeout = 10 * 1000
    };

    enum { kInvalidOpSize = -1 };

}

namespace
{
    const QHostAddress kMulticastGroupAddress = QHostAddress("239.255.11.11");
    const quint16 kMulticastGroupPort = 5007;
    
    const QByteArray kSearchRequestBody("{ magic: \"7B938F06-ACF1-45f0-8303-98AA8057739A\" }");
    const int kSearchRequestSize = kSearchRequestBody.size();
    
    typedef QHash<QString, std::function<void (const QJsonObject &object
        , rtu::BaseServerInfo &info)> > KeyParserContainer;
    const KeyParserContainer parser = []() -> KeyParserContainer
    {
        KeyParserContainer result;
        result.insert(kIDTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.id = QUuid(object.value(kIDTag).toString()); });
        result.insert(kSeedTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.id = QUuid(object.value(kSeedTag).toString()); });
        result.insert(kNameTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.name = object.value(kNameTag).toString(); });
        result.insert(kSystemNameTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.systemName = object.value(kSystemNameTag).toString(); });
        result.insert(kPortTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.port = object.value(kPortTag).toInt(); });
            
            
        result.insert(kFlagsTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            {
                typedef QPair<QString, rtu::Constants::ServerFlag> TextFlagsInfo;
                static const TextFlagsInfo kKnownFlags[] = 
                {
                    TextFlagsInfo("SF_timeCtrl", rtu::Constants::ServerFlag::AllowChangeDateTimeFlag)
                    , TextFlagsInfo("SF_IfListCtrl", rtu::Constants::ServerFlag::AllowIfConfigFlag)
                    , TextFlagsInfo("SF_AutoSystemName" , rtu::Constants::ServerFlag::IsFactoryFlag)
                };
                
                info.flags = rtu::Constants::ServerFlag::NoFlags;
                const QString textFlags = object.value(kFlagsTag).toString();
                for (const TextFlagsInfo &flagInfo: kKnownFlags)
                {
                    if (textFlags.contains(flagInfo.first))
                        info.flags |= flagInfo.second;
                }
            });
        return result;
    }();
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

        const QStringList &keys = jsonObject.keys();
        for (const auto &key: keys)
        {
            const auto itHandler = parser.find(key);
            if (itHandler != parser.end())
                (*itHandler)(jsonObject, info);
        }
        
        return true;
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
    void checkOutdatedServers();

    void updateServers();
    
    typedef QSharedPointer<QUdpSocket> SocketPtr;
    bool readData(const SocketPtr &socket);

private:
    typedef QHash<QHostAddress, SocketPtr> SocketsContainer;
    typedef QPair<qint64, rtu::BaseServerInfo> ServerInfoData;
    typedef QHash<QUuid, ServerInfoData> ServerDataContainer;

    rtu::ServersFinder *m_owner;
    SocketsContainer m_sockets;
    ServerDataContainer m_infos;
    rtu::IDsVector m_waitedServers;
    QTimer m_timer;
    QElapsedTimer m_msecCounter;
};

rtu::ServersFinder::Impl::Impl(rtu::ServersFinder *owner)
    : QObject(owner)
    , m_owner(owner)
    , m_sockets()
    , m_infos()
    , m_timer()
    , m_msecCounter()
{
    updateServers();
    
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
        const qint64 lastUpdateTime = infoData.first;
        const BaseServerInfo &info = infoData.second;
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


void rtu::ServersFinder::Impl::updateServers()
{
    checkOutdatedServers();
    
    const auto allAddresses = QNetworkInterface::allAddresses();
    for(const auto &address: allAddresses)
    {
        if ((address.protocol() != QAbstractSocket::IPv4Protocol))
            continue;

        SocketsContainer::iterator &it = m_sockets.find(address);
        if (it == m_sockets.end())
            it = m_sockets.insert(address, SocketPtr(new QUdpSocket()));
        
        const SocketPtr &socket = *it;
        if (socket->state() != QAbstractSocket::BoundState)
        {
            socket->close();
            if (!socket->bind(address))
                continue;
        }

        while(socket->hasPendingDatagrams() && readData(socket))
        {
        }

        int written = 0;
        while((socket->state() == QAbstractSocket::BoundState) && (written != kSearchRequestBody.size()))
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

bool rtu::ServersFinder::Impl::readData(const rtu::ServersFinder::Impl::SocketPtr &socket)
{
    const int pendingDataSize = socket->pendingDatagramSize();
    QHostAddress sender;
    quint16 senderPort;
    
    enum { kZeroSymbol = 0 };
    QByteArray data(pendingDataSize, kZeroSymbol);

    int readBytes = 0;
    while ((socket->state() == QAbstractSocket::BoundState) && (readBytes != pendingDataSize))
    {
        const int read = socket->readDatagram(data.data() + readBytes
            , pendingDataSize - readBytes, &sender, &senderPort);

        if (read == kInvalidOpSize)
            return false;

        readBytes += read;
    }

    if (readBytes != pendingDataSize)
    {
        socket->close();
        return false;
    }
    
    BaseServerInfo info;

    if (!parseUdpPacket(data, info) )
        return true;


    info.hostAddress = sender.toString();

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
        it.value().first = timestamp; /// Update alive info
        if (info != it->second)
        {
            it.value().second.name = info.name;
            it.value().second.systemName = info.systemName;
            it.value().second.port = info.port;
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
