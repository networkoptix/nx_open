#ifdef ENABLE_TEST_CAMERA

#include "testcamera_resource_searcher.h"
#include "testcamera_resource.h"
#include <nx/network/nettools.h>
#include "utils/common/sleep.h"
#include "utils/common/util.h"
#include <nx/vms/testcamera/test_camera_ini.h>
#include <nx/utils/log/log.h>
#include <nx/network/url/url_builder.h>

static const qint64 SOCK_UPDATE_INTERVAL = 1000000ll * 60 * 5;

using nx::vms::testcamera::ini;

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
        for (const auto& address: nx::network::allLocalAddresses(nx::network::ipV4))
        {
            DiscoveryInfo info(nx::network::SocketFactory::createDatagramSocket().release(),
                QHostAddress(address.toString()));
            if (info.sock->bind(address.toString(), 0))
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
    ini().reload();
    const QByteArray testCameraFindMessage = ini().discoveryMessage + QByteArray("\n");
    for (const DiscoveryInfo& info: m_sockList)
    {
        info.sock->sendTo(
            testCameraFindMessage.constData(), testCameraFindMessage.size(),
            nx::network::BROADCAST_ADDRESS, ini().discoveryPort);
    }
}

QnResourceList QnTestCameraResourceSearcher::findResources()
{
    if (updateSocketList())
    {
        sendBroadcast();
        QnSleep::msleep(1000);
    }

    QnResourceList result;
    QSet<QString> processedMacs;

    const QByteArray testCameraIdMessage =
        ini().discoveryResponseMessage + QByteArray("\n");

    for (const DiscoveryInfo& info: m_sockList)
    {
        nx::network::AbstractDatagramSocket* sock = info.sock;
        while (sock->hasData())
        {
            QByteArray responseData;
            responseData.resize(nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE);

            nx::network::SocketAddress remoteEndpoint;
            const int bytesRead = sock->recvFrom(
                responseData.data(), responseData.size(), &remoteEndpoint);
            if (bytesRead < 1)
                continue;
            const QList<QByteArray> params = responseData.left(bytesRead).split(';');
            if (params[0] != testCameraIdMessage || params.size() < 3)
                continue;

            const int videoPort = params[1].toInt();
            for (int paramIndex = 2; paramIndex < params.size(); ++paramIndex)
            {
                const QString mac = params[paramIndex];
                if (processedMacs.contains(mac))
                    continue;

                // TODO: #mshevchenko CURRENT
                const QnTestCameraResourcePtr resource(new QnTestCameraResource(serverModule()));
                const QString model = QLatin1String(QnTestCameraResource::kModel);
                const QnUuid resourceTypeId =
                    qnResTypePool->getResourceTypeId(manufacturer(), model);
                if (resourceTypeId.isNull())
                    continue;

                const nx::utils::Url url = nx::network::url::Builder()
                    .setScheme("tcp")
                    .setHost(remoteEndpoint.address.toString())
                    .setPort(videoPort)
                    .setPath(mac);

                resource->setTypeId(resourceTypeId);
                resource->setName(model);
                resource->setModel(model);
                resource->setMAC(nx::utils::MacAddress(mac));
                resource->setUrl(url.toString());

                NX_VERBOSE(this, "Found test camera %1 (URL: %2)", resource, url);
                processedMacs << mac;
                result << resource;
            }
        }
    }

    sendBroadcast();

    return result;
}

QnResourcePtr QnTestCameraResourceSearcher::createResource(
    const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    const QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        NX_WARNING(this, lm("No resource type for id: %1").arg(resourceTypeId));
        return result;
    }

    if (resourceType->getManufacturer() != manufacturer())
    {
        NX_VERBOSE(this, "Manufacturer %1 != %2", resourceType->getManufacturer(), manufacturer());
        return result;
    }

    result = QnVirtualCameraResourcePtr(new QnTestCameraResource(serverModule()));
    result->setTypeId(resourceTypeId);

    NX_DEBUG(this, "Create test camera resource [%1], type id: [%2]",
        result, resourceTypeId);
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
