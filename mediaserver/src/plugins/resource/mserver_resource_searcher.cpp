#include "mserver_resource_searcher.h"

#include <business/events/mserver_conflict_business_event.h>
#include "utils/network/nettools.h"
#include "utils/network/system_socket.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h" /* For DEFAULT_APPSERVER_HOST. */

#include <core/dataprovider/live_stream_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

#include <business/business_event_connector.h>

#include <media_server/serverutil.h>
#include <media_server/settings.h>



static quint16 DISCOVERY_PORT = 54013;
static const int UPDATE_IF_LIST_INTERVAL = 1000 * 60;
static QString groupAddress(QLatin1String("224.0.1.243"));
static const QByteArray guidStr("{756E732D-0FB1-4f91-8CE0-381D1A3F84E8}");

QByteArray localAppServerHost()
{
    QByteArray result = MSSettings::roSettings()->value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString().toUtf8();
    if (isLocalAppServer(result)) {
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
        for(QnResourceConsumer* consumer: netRes->getAllConsumers())
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

    static QStringList getLocalUsingCameras()
    {
        QStringList result;
        QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(serverGuid()));
        if (mediaServer) {
            QnResourceList resList = qnResPool->getResourcesWithParentId(mediaServer->getId());
            for (int i = 0; i < resList.size(); ++i) {
                QnNetworkResourcePtr netRes = resList[i].dynamicCast<QnNetworkResource>();
                if (netRes && hasRunningLiveProvider(netRes))
                    result << netRes->getPhysicalId();
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
        QStringList cameras = getLocalUsingCameras();
        for (const QString &camera: cameras)
            result.append(camera.toUtf8());
        return listToByteArray(result);
    }

    void fillCameraConflictList(QStringList &result) const {
        QStringList localCameras = getLocalUsingCameras();
        if (localCameras.isEmpty())
            return;

        for (const QByteArray &camera: m_cameras) {
            QString cam = QString::fromUtf8(camera);
            if (localCameras.contains(cam) && !result.contains(cam))
                result << cam;
        }
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

QnMServerResourceSearcher::QnMServerResourceSearcher()
{
}

void QnMServerResourceSearcher::run()
{
    initSystemThreadId();
    updateSocketList();

    while (!m_needStop)
    {
        readDataFromSocket();

    }
}

void QnMServerResourceSearcher::updateSocketList()
{
    deleteSocketList();
    for (const QnInterfaceAndAddr& iface: getAllIPv4Interfaces())
    {
        UDPSocket* socket = new UDPSocket();
        QString localAddress = iface.address.toString();
        //if (socket->bindToInterface(iface))
        if (socket->bind(SocketAddress(iface.address.toString())))
        {
            socket->setMulticastIF(localAddress);
            m_socketList << socket;
            m_localAddressList << localAddress;
        }
        else {
            delete socket;
        }
    }

    m_receiveSocket.reset( new UDPSocket() );
    m_receiveSocket->setReuseAddrFlag(true);
    m_receiveSocket->bind(SocketAddress(HostAddress::anyHost, DISCOVERY_PORT));

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

    m_receiveSocket.reset();
}

void QnMServerResourceSearcher::readDataFromSocket()
{
    if (m_socketLifeTime.elapsed() > UPDATE_IF_LIST_INTERVAL)
        updateSocketList();

    for (int i = 0; i < m_socketList.size(); ++i)
    {
        AbstractDatagramSocket* sock = m_socketList[i];

        // send request for next read
        QByteArray datagram = DiscoveryPacket::getRequest(m_appServerGuid);
        sock->sendTo(datagram.data(), datagram.size(), groupAddress, DISCOVERY_PORT);
    }

    for (int i = 0; i < 600 && !m_needStop; ++i)
        QnSleep::msleep(100);

    QnCameraConflictList conflicts;
    readSocketInternal(m_receiveSocket.get(), conflicts);
    if (!conflicts.camerasByServer.isEmpty()) {
        conflicts.sourceServer = localAppServerHost();
        QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(serverGuid()));
        if (mediaServer)
            qnBusinessRuleConnector->at_mediaServerConflict(mediaServer, qnSyncTime->currentUSecsSinceEpoch(), conflicts);
    }
}

void QnMServerResourceSearcher::readSocketInternal(AbstractDatagramSocket* socket, QnCameraConflictList& conflictList)
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
            if (packet.isValidPacket() && packet.appServerGuid() != m_appServerGuid) {
                QStringList cameras = conflictList.camerasByServer.contains(packet.appServerHost())
                    ? conflictList.camerasByServer[packet.appServerHost()]
                    : QStringList();
                packet.fillCameraConflictList(cameras);
                if (!cameras.isEmpty())
                    conflictList.camerasByServer[packet.appServerHost()] = cameras;
            }
        }
    }
}

void QnMServerResourceSearcher::setAppPServerGuid(const QByteArray& appServerGuid)
{
    m_appServerGuid = appServerGuid;
}
