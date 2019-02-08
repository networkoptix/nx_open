#ifdef ENABLE_TEST_CAMERA

#include "testcamera_resource_searcher.h"
#include "testcamera_resource.h"
#include <nx/network/nettools.h>
#include "utils/common/sleep.h"
#include "utils/common/util.h"
#include <core/resource/test_camera_ini.h>
#include <nx/utils/log/log.h>

static const qint64 SOCK_UPDATE_INTERVAL = 1000000ll * 60 * 5;

QnTestCameraResourceSearcher::QnTestCameraResourceSearcher(QnMediaServerModule* serverModule)
    :
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QnTestCameraResourceSearcher::~QnTestCameraResourceSearcher()
{
    clearSocketList();
}

void QnTestCameraResourceSearcher::clearSocketList()
{
    for(const DiscoveryInfo& info: m_sockList)
        delete info.sock;
    m_sockList.clear();
}

bool QnTestCameraResourceSearcher::updateSocketList()
{
    qint64 curretTime = getUsecTimer();
    if (curretTime - m_sockUpdateTime > SOCK_UPDATE_INTERVAL)
    {
        clearSocketList();
        for (const nx::network::QnInterfaceAndAddr& iface: nx::network::getAllIPv4Interfaces())
        {
            DiscoveryInfo info(
                nx::network::SocketFactory::createDatagramSocket().release(), iface.address);
            if (info.sock->bind(iface.address.toString(), 0))
                m_sockList << info;
            else
                delete info.sock;
        }
        m_sockUpdateTime = curretTime;
        return true;
    }
    return false;
}

void QnTestCameraResourceSearcher::sendBroadcast()
{
    testCameraIni().reload();
    for (const DiscoveryInfo& info: m_sockList)
    {
        info.sock->sendTo(
            testCameraIni().findMessage, strlen(testCameraIni().findMessage),
            nx::network::BROADCAST_ADDRESS, testCameraIni().discoveryPort);
    }
}

QnResourceList QnTestCameraResourceSearcher::findResources(void)
{
    if (updateSocketList()) {
        sendBroadcast();
        QnSleep::msleep(1000);
    }

    QSet<QHostAddress> foundDevSet; // to avoid duplicates
    QMap<QString, QnResourcePtr> resources;
    QSet<QString> processedMac;

    for(const DiscoveryInfo& info: m_sockList)
    {
        nx::network::AbstractDatagramSocket* sock = info.sock;
        while (sock->hasData())
        {
            QByteArray responseData;
            responseData.resize(nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE);

            nx::network::SocketAddress remoteEndpoint;
            int readed = sock->recvFrom(responseData.data(), responseData.size(), &remoteEndpoint);
            if (readed < 1)
                continue;
            QList<QByteArray> params = responseData.left(readed).split(';');
            if (params[0] != testCameraIni().idMessage || params.size() < 3)
                continue;

            int videoPort = params[1].toInt();
            for (int j = 2; j < params.size(); ++j)
            {
                QnTestCameraResourcePtr resource (new QnTestCameraResource(serverModule()));
                QString model = QLatin1String(QnTestCameraResource::kModel);
                QnUuid rt = qnResTypePool->getResourceTypeId(manufacturer(), model);
                if (rt.isNull())
                    continue;

                QLatin1String s(params[j]);

                resource->setTypeId(rt);
                resource->setName(model);
                resource->setModel(model);
                QString mac(s);
                if (processedMac.contains(mac))
                    continue;
                processedMac << mac;

                resource->setMAC(nx::utils::MacAddress(mac));
                resource->setUrl(QLatin1String("tcp://") + remoteEndpoint.address.toString()
                    + QLatin1Char(':') + QString::number(videoPort) + QLatin1Char('/')
                    + QLatin1String(params[j]));
                resources.insert(mac, resource);
            }
        }
    }
    QnResourceList rez;
    for(const QnResourcePtr& res: resources.values())
        rez << res;

    sendBroadcast();

    return rez;
}

QnResourcePtr QnTestCameraResourceSearcher::createResource(
    const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        NX_WARNING(this, lm("No resource type for id: %1").arg(resourceTypeId));
        return result;
    }

    if (resourceType->getManufacturer() != manufacturer())
    {
        //qDebug() << "Manufature " << resourceType->getManufacturer() << " != " << manufacturer();
        return result;
    }

    result = QnVirtualCameraResourcePtr(new QnTestCameraResource(serverModule()));
    result->setTypeId(resourceTypeId);

    NX_DEBUG(this, lm("Create test camera resource, type id: %1").arg(resourceTypeId));
    return result;

}

QString QnTestCameraResourceSearcher::manufacturer() const
{
    return QLatin1String(QnTestCameraResource::kManufacturer);
}

QList<QnResourcePtr> QnTestCameraResourceSearcher::checkHostAddr(const nx::utils::Url& url,
    const QAuthenticator& /*auth*/, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )
        return QList<QnResourcePtr>(); //< searching if only host is present, not specific protocol

    return QList<QnResourcePtr>();
}

#endif // #ifdef ENABLE_TEST_CAMERA
