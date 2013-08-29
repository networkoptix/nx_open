#include "mserver_resource_searcher.h"
#include "core/resource/resource.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "serverutil.h"
#include "utils/common/synctime.h"
#include <business/business_event_connector.h>
#include "settings.h"
#include "core/resource_managment/resource_pool.h"
#include "core/dataprovider/live_stream_provider.h"

static quint16 DISCOVERY_PORT = 54013;
static const int UPDATE_IF_LIST_INTERVAL = 1000 * 60;
static QString groupAddress(QLatin1String("224.0.1.243"));
static const QByteArray guidStr("{756E732D-0FB1-4f91-8CE0-381D1A3F84E8}");

QByteArray localAppServerHost()
{
    QByteArray result = qSettings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString().toUtf8();
    if (result == "localhost" || result == "127.0.0.1")
    {
        QList<QnInterfaceAndAddr> interfaces = getAllIPv4Interfaces();
        if (!interfaces.isEmpty())
            result = interfaces[0].address.toString().toUtf8();
    }
    return result;
}

class DiscoveryPacket
{
    static QByteArray listToByteArray(const QList<QByteArray>& list)
    {
        QByteArray result;
        for (int i = 0; i < list.size(); ++i) {
            if (i > 0)
                result.append('\0');
            result.append(list[i]);
        }
        return result;
    }

    static bool hasRunningLiveProvider(QnNetworkResourcePtr netRes)
    {
        bool rez = false;
        netRes->lockConsumers();
        foreach(QnResourceConsumer* consumer, netRes->getAllConsumers())
        {
            QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
            if (lp)
            {
                QnLongRunnable* lr = dynamic_cast<QnLongRunnable*>(lp);
                if (lr && lr->isRunning()) {
                    rez = true;
                    break;
                }
            }
        }

        netRes->unlockConsumers();
        return rez;
    }


private:
    QByteArray m_data;
    QByteArray m_appServerGuid;
    QByteArray m_appServerHost;
    QByteArray m_guid;
    QList<QByteArray> m_cameras;

public:
    DiscoveryPacket(const QByteArray data)
    {
        m_data = data;
        QList<QByteArray> lines = m_data.split((char) 0);
        if (lines.size() >= 3) {
            m_guid = lines[0];
            m_appServerGuid = lines[1];
            m_appServerHost = lines[2];
            for (int i = 3; i < lines.size(); ++i)
                m_cameras << lines[i];
        }
    }

    static QList<QByteArray> getLocalUsingCameras()
    {
        QList<QByteArray> result;
        QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceByGuid(serverGuid()));
        if (mediaServer) {
            QnResourceList resList = qnResPool->getResourcesWithParentId(mediaServer->getId());
            for (int i = 0; i < resList.size(); ++i) {
                QnNetworkResourcePtr netRes = resList[i].dynamicCast<QnNetworkResource>();
                if (netRes && hasRunningLiveProvider(netRes))
                    result << netRes->getPhysicalId().toUtf8();
            }
        }
        return result;
    }

    bool isValidPacket() const {
        return m_guid == guidStr;
    }

    QByteArray appServerHost() const {
        return m_appServerHost;
    }

    QByteArray appServerGuid() const {
        return m_appServerGuid;
    }

    static QByteArray getRequest(const QByteArray& appServerGuid)
    {
        QList<QByteArray> result;
        result << guidStr;
        result << appServerGuid;
        result << localAppServerHost();
        QList<QByteArray> cameras = getLocalUsingCameras();
        result.append(cameras);

        return listToByteArray(result);
    }

    bool isCameraConflict() const {
        QList<QByteArray> localCameras = getLocalUsingCameras();
        for (int i = 0; i < m_cameras.size(); ++i) {
            if (localCameras.contains(m_cameras[i]))
                return true;
        }
        return false;
    }
};


//Q_GLOBAL_STATIC(QnMServerResourceSearcher, inst)
static QnMServerResourceSearcher* staticInstance = NULL;

void QnMServerResourceSearcher::initStaticInstance( QnMServerResourceSearcher* inst )
{
    staticInstance = inst;
}

QnMServerResourceSearcher* QnMServerResourceSearcher::instance()
{
    return staticInstance;
}

QnMServerResourceSearcher::~QnMServerResourceSearcher()
{
    stop();
    deleteSocketList();
}

QnMServerResourceSearcher::QnMServerResourceSearcher(): 
    QnLongRunnable(),
    m_receiveSocket(0)
{
}

void QnMServerResourceSearcher::run()
{
    saveSysThreadID();
    updateSocketList();

    while (!m_needStop)
    {
        readDataFromSocket();

    }
}

void QnMServerResourceSearcher::updateSocketList()
{
    deleteSocketList();
    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        UDPSocket* socket = new UDPSocket();
        QString localAddress = iface.address.toString();
        if (socket->bindToInterface(iface))
        {
            socket->setMulticastIF(localAddress);
            m_socketList << socket;
            m_localAddressList << localAddress;
        }
        else {
            delete socket;
        }
    }

    m_receiveSocket = new UDPSocket();
    m_receiveSocket->setReuseAddrFlag(true);
    m_receiveSocket->setLocalPort(DISCOVERY_PORT);

    for (int i = 0; i < m_localAddressList.size(); ++i)
        m_receiveSocket->joinGroup(groupAddress, m_localAddressList[i]);

    m_socketLifeTime.restart();
}

void QnMServerResourceSearcher::deleteSocketList()
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

void QnMServerResourceSearcher::readDataFromSocket()
{
    if (m_socketLifeTime.elapsed() > UPDATE_IF_LIST_INTERVAL)
        updateSocketList();

    for (int i = 0; i < m_socketList.size(); ++i)
    {
        UDPSocket* sock = m_socketList[i];

        // send request for next read
        QByteArray datagram = DiscoveryPacket::getRequest(m_appServerGuid);
        sock->sendTo(datagram.data(), datagram.size(), groupAddress, DISCOVERY_PORT);
    }

    for (int i = 0; i < 600 && !m_needStop; ++i)
        QnSleep::msleep(100);

    QSet<QByteArray> conflictList;
    readSocketInternal(m_receiveSocket, conflictList);
    if (!conflictList.isEmpty()) {
        QList<QByteArray> cList = conflictList.toList();
        cList.insert(0, localAppServerHost());
        QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceByGuid(serverGuid()));
        if (mediaServer)
            qnBusinessRuleConnector->at_mediaServerConflict(mediaServer, qnSyncTime->currentUSecsSinceEpoch(), cList);
    }
}

void QnMServerResourceSearcher::readSocketInternal(UDPSocket* socket, QSet<QByteArray>& conflictList)
{
    quint8 tmpBuffer[1024*16];
    while (socket->hasData())
    {
        QString remoteAddress;
        quint16 remotePort;
        int datagramSize = socket->recvFrom(tmpBuffer, sizeof(tmpBuffer), remoteAddress, remotePort);
        if (datagramSize > 0) {
            QByteArray responseData((const char*) tmpBuffer, datagramSize);
            DiscoveryPacket packet(responseData);
            if (packet.isValidPacket() && packet.appServerGuid() != m_appServerGuid && packet.isCameraConflict())
                conflictList.insert(packet.appServerHost());
        }
    }
}

void QnMServerResourceSearcher::setAppPServerGuid(const QByteArray& appServerGuid)
{
    m_appServerGuid = appServerGuid;
}
