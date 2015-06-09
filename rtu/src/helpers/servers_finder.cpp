
#include "servers_finder.h"

#include <functional>

#include <QTimer>
#include <QDateTime>
#include <QUdpSocket>

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkInterface>
#include <QNetworkAccessManager>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <server_info.h>
#include <requests/requests.h>

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
        kUpdateServersInfoInterval = 500
        , kServerAliveTimeout = 100 * 1000
    };
}

namespace
{
    typedef std::function<void (const rtu::BaseServerInfo &)> OnServerActionFunctionType;
    typedef std::function<void (const rtu::IDsVector &)> OnRemovedFunctionType;

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
                    TextFlagsInfo("SF_timeCtrl", rtu::Constants::ServerFlag::kAllowChangeDateTime)
                    , TextFlagsInfo("SF_IfListCtrl", rtu::Constants::ServerFlag::kAllowChangeInterfaceSettings)
                    , TextFlagsInfo("SF_AutoSystemName" , rtu::Constants::ServerFlag::kIsFactory)
                };
                
                info.flags = rtu::Constants::ServerFlag::kNoFlags;
                const QString textFlags = object.value(kFlagsTag).toString();
                for (const TextFlagsInfo &flagInfo: kKnownFlags)
                {
                    if (textFlags.contains(flagInfo.first));
                        info.flags &= flagInfo.second;
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
        
        if (!jsonObject.value("systemName").toString().contains("system_"))
            return false;
        
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
    Impl(const OnServerActionFunctionType &onAddedFunction
         , const OnServerActionFunctionType &onChangedFunction
         , const OnRemovedFunctionType &onRemovedFunction);
    
    ~Impl();

private:
    void checkOutdatedServers();

    void updateServers();
    
    typedef QSharedPointer<QUdpSocket> SocketPtr;
    void readData(const SocketPtr &socket);
    
private:
    
    typedef QHash<QHostAddress, SocketPtr> SocketsContainer;
    typedef QPair<int, rtu::BaseServerInfo> ServerInfoData;
    typedef QHash<QUuid, ServerInfoData> ServerDataContainer;
    
    const OnServerActionFunctionType m_onAddedHandler;
    const OnServerActionFunctionType m_onChangedHandler;
    const OnRemovedFunctionType m_onRemovedHandler;
    
    SocketsContainer m_sockets;
    ServerDataContainer m_infos;
    QTimer m_timer;    
};

rtu::ServersFinder::Impl::Impl(const OnServerActionFunctionType &onAddedFunction
    , const OnServerActionFunctionType &onChangedFunction
    , const OnRemovedFunctionType &onRemovedFunction)
    : m_onAddedHandler(onAddedFunction)
    , m_onChangedHandler(onChangedFunction)
    , m_onRemovedHandler(onRemovedFunction)
    
    , m_sockets()
    , m_infos()
    , m_timer()
{
    updateServers();
    
    QObject::connect(&m_timer, &QTimer::timeout, this, &Impl::updateServers);
    m_timer.setInterval(kUpdateServersInfoInterval);
    m_timer.start();
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
    const int currentTime = QDateTime::currentMSecsSinceEpoch();
    
    IDsVector forRemove;
    for(const auto &infoData: m_infos)
    {
        const int lastUpdateTime = infoData.first;
        const BaseServerInfo &info = infoData.second;
        if ((currentTime - lastUpdateTime) > kServerAliveTimeout)
            forRemove.push_back(info.id);
    }
    
    if (!forRemove.empty())
    {
        for(const auto &id: forRemove)
            m_infos.remove(id);
        
        m_onRemovedHandler(forRemove);
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

        const auto it = m_sockets.find(address);
        if (it == m_sockets.end())
        {
            SocketPtr socket(new QUdpSocket());
            if (socket->bind(address))
            {
                QObject::connect(socket.data(), &QUdpSocket::readyRead, [this, socket](){ readData(socket); });
                m_sockets.insert(address, socket);
                
            }
        }
        else if ((*it)->state() != QAbstractSocket::BoundState)
        {
            const SocketPtr &socket = *it;
            socket->close();
            socket->bind(address);
        }
        
        for(const auto &socket: m_sockets)
        {
            int written = 0;
            while((socket->state() == QAbstractSocket::BoundState) && (written != kSearchRequestBody.size()))
            {
                written += socket->writeDatagram(kSearchRequestBody.data() + written, kSearchRequestSize - written
                    , kMulticastGroupAddress, kMulticastGroupPort);
            }
            if (written != kSearchRequestSize)
                socket->close();
        }
    }
}

void rtu::ServersFinder::Impl::readData(const rtu::ServersFinder::Impl::SocketPtr &socket)
{
    const int pendingDataSize = socket->pendingDatagramSize();
    QHostAddress sender;
    quint16 senderPort;
    
    enum { kZeroSymbol = 0 };
    QByteArray data(pendingDataSize, kZeroSymbol);

    int readBytes = 0;
    while ((socket->state() == QAbstractSocket::BoundState) && (readBytes != pendingDataSize))
    {
        readBytes += socket->readDatagram(data.data() + readBytes, pendingDataSize - readBytes, &sender, &senderPort);
    }
    
    if (readBytes != pendingDataSize)
    {
        socket->close();
        return;
    }
    
    BaseServerInfo info;
    if (!parseUdpPacket(data, info) )
        return;

    info.hostAddress = sender.toString();
    
    const auto it = m_infos.find(info.id);
    const int timestamp = QDateTime::currentMSecsSinceEpoch();
    if (it == m_infos.end()) /// If new server multicast arrived
    {
        m_infos.insert(info.id, ServerInfoData(timestamp, info));
        m_onAddedHandler(info);
    }
    else
    {
        it.value().first = QDateTime::currentMSecsSinceEpoch(); /// Update alive info
        if (info != it->second)
            m_onChangedHandler(info);
    }
}

///

rtu::ServersFinder::Holder rtu::ServersFinder::create()
{
    return Holder(new ServersFinder());
}

rtu::ServersFinder::ServersFinder()
    : m_impl(new Impl(
        [this](const BaseServerInfo &info){ emit onAddedServer(info);}
        , [this](const BaseServerInfo &info){ emit onChangedServer(info);}
        , [this](const IDsVector &removed) { emit onRemovedServers(removed); }))
{
}

rtu::ServersFinder::~ServersFinder()
{
    
}
