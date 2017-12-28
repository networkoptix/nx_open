#include "mserver_resource_searcher.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <providers/live_stream_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include <media_server/media_server_module.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h> /* For DEFAULT_APPSERVER_HOST. */
#include <nx/mediaserver/event/event_connector.h>
#include <nx/vms/event/events/server_conflict_event.h>
#include <nx/network/nettools.h>
#include <nx/network/system_socket.h>

using nx::network::UDPSocket;

static quint16 DISCOVERY_PORT = 54013;
static const int UPDATE_IF_LIST_INTERVAL = 1000 * 60;
static QString groupAddress(QLatin1String("224.0.1.243"));
static const QByteArray guidStr("{756E732D-0FB1-4f91-8CE0-381D1A3F84E8}");

QByteArray localAppServerHost()
{
    QByteArray result = qnServerModule->roSettings()->value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString().toUtf8();
    if (isLocalAppServer(result)) {
        QList<nx::network::QnInterfaceAndAddr> interfaces = nx::network::getAllIPv4Interfaces();
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
    QnUuid m_systemId;
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
            m_systemId = QnUuid::fromStringSafe(lines[1]);
            if (m_systemId.isNull() && !lines[1].isEmpty())
                m_systemId = guidFromArbitraryData(lines[1]); //< for compatibility with previous versions
            m_appServerHost = lines[2];
            for (int i = 3; i < lines.size(); ++i)
                m_cameras << lines[i];
        }
    }

    static QStringList getLocalUsingCameras(const QnCommonModule* commonModule)
    {
        QStringList result;
        const auto& pool = commonModule->resourcePool();
        QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (
            pool->getResourceById(commonModule->moduleGUID()));
        if (mediaServer)
        {
            QnResourceList resList = pool->getResourcesWithParentId(mediaServer->getId());
            for (int i = 0; i < resList.size(); ++i) {
                QnNetworkResourcePtr netRes = resList[i].dynamicCast<QnNetworkResource>();
                if (netRes && hasRunningLiveProvider(netRes))
                    result << netRes->getPhysicalId();
            }
        }
        return result;
    }

    bool isValidPacket() const
    {
        return m_guid == guidStr;
    }

    QByteArray appServerHost() const
    {
        return m_appServerHost;
    }

    QnUuid systemId() const
    {
        return m_systemId;
    }

    static QByteArray getRequest(const QnCommonModule* commonModule)
    {
        QList<QByteArray> result;
        result << guidStr;
        result << commonModule->globalSettings()->localSystemId().toByteArray();
        result << localAppServerHost();
        QStringList cameras = getLocalUsingCameras(commonModule);
        for (const QString &camera: cameras)
            result.append(camera.toUtf8());
        return listToByteArray(result);
    }

    void fillCameraConflictList(QnCommonModule* commonModule, QStringList &result) const
    {
        QStringList localCameras = getLocalUsingCameras(commonModule);
        if (localCameras.isEmpty())
            return;

        for (const QByteArray &camera: m_cameras) {
            QString cam = QString::fromUtf8(camera);
            if (localCameras.contains(cam) && !result.contains(cam))
                result << cam;
        }
    }
};

QnMServerResourceSearcher::QnMServerResourceSearcher(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

QnMServerResourceSearcher::~QnMServerResourceSearcher()
{
    stop();
    deleteSocketList();
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
    for (const nx::network::QnInterfaceAndAddr& iface: nx::network::getAllIPv4Interfaces())
    {
        UDPSocket* socket = new UDPSocket(AF_INET);
        QString localAddress = iface.address.toString();
        //if (socket->bindToInterface(iface))
        if (socket->bind(nx::network::SocketAddress(iface.address.toString())))
        {
            socket->setMulticastIF(localAddress);
            m_socketList << socket;
            m_localAddressList << localAddress;
        }
        else {
            delete socket;
        }
    }

    m_receiveSocket.reset( new UDPSocket(AF_INET) );
    m_receiveSocket->setReuseAddrFlag(true);
    m_receiveSocket->bind(nx::network::SocketAddress(nx::network::HostAddress::anyHost, DISCOVERY_PORT));

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
        nx::network::AbstractDatagramSocket* sock = m_socketList[i];

        // send request for next read
        QByteArray datagram = DiscoveryPacket::getRequest(commonModule());
        sock->sendTo(datagram.data(), datagram.size(), groupAddress, DISCOVERY_PORT);
    }

    for (int i = 0; i < 600 && !m_needStop; ++i)
        QnSleep::msleep(100);

    QnCameraConflictList conflicts;
    readSocketInternal(m_receiveSocket.get(), conflicts);
    if (!conflicts.camerasByServer.isEmpty()) {
        conflicts.sourceServer = localAppServerHost();
        QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (resourcePool()->getResourceById(serverGuid()));
        if (mediaServer)
            qnEventRuleConnector->at_serverConflict(mediaServer, qnSyncTime->currentUSecsSinceEpoch(), conflicts);
    }
}

void QnMServerResourceSearcher::readSocketInternal(nx::network::AbstractDatagramSocket* socket, QnCameraConflictList& conflictList)
{
    quint8 tmpBuffer[1024*16];
    while (socket->hasData())
    {
        nx::network::SocketAddress remoteEndpoint;
        int datagramSize = socket->recvFrom(tmpBuffer, sizeof(tmpBuffer), &remoteEndpoint);
        if (datagramSize > 0) {
            QByteArray responseData((const char*) tmpBuffer, datagramSize);
            DiscoveryPacket packet(responseData);
            if (packet.isValidPacket() && packet.systemId() != qnGlobalSettings->localSystemId())
            {
                QStringList cameras = conflictList.camerasByServer.contains(packet.appServerHost())
                    ? conflictList.camerasByServer[packet.appServerHost()]
                    : QStringList();
                packet.fillCameraConflictList(commonModule(), cameras);
                if (!cameras.isEmpty())
                    conflictList.camerasByServer[packet.appServerHost()] = cameras;
            }
        }
    }
}
