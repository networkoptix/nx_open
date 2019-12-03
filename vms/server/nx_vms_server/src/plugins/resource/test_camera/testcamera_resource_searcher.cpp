#include "testcamera_resource_searcher.h"
#if defined(ENABLE_TEST_CAMERA)

#include <nx/kit/utils.h>
#include <nx/network/nettools.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/vms/testcamera/discovery_response.h>
#include <nx/vms/testcamera/test_camera_ini.h>
#include <utils/common/sleep.h>
#include <utils/common/util.h>

#include "testcamera_resource.h"

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
    const qint64 currentTime = getUsecTimer();
    if (currentTime - m_sockUpdateTime <= SOCK_UPDATE_INTERVAL)
        return false;

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
    m_sockUpdateTime = currentTime;
    return true;
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

    QnResourceList resources;
    QSet<nx::utils::MacAddress> processedMacAddresses;

    for (const DiscoveryInfo& info: m_sockList)
    {
        nx::network::AbstractDatagramSocket* const socket = info.sock;
        while (socket->hasData())
        {
            QByteArray discoveryResponseMessage;
            discoveryResponseMessage.resize(
                nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE);

            nx::network::SocketAddress remoteEndpoint;
            const int bytesRead = socket->recvFrom(
                discoveryResponseMessage.data(), discoveryResponseMessage.size(), &remoteEndpoint);
            if (bytesRead < 1)
                continue;

            discoveryResponseMessage.resize(bytesRead);

            processDiscoveryResponseMessage(
                discoveryResponseMessage,
                remoteEndpoint.address.toString(),
                &resources,
                &processedMacAddresses);
        }
    }

    sendBroadcast();

    return resources;
}

void QnTestCameraResourceSearcher::processDiscoveryResponseMessage(
    const QByteArray& discoveryResponseMessage,
    const QString& serverAddress,
    QnResourceList* resources,
    QSet<nx::utils::MacAddress>* processedMacAddresses) const
{
    QString errorMessage;
    const nx::vms::testcamera::DiscoveryResponse discoveryResponse(
        discoveryResponseMessage, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        NX_DEBUG(this,
            "Ignoring received testcamera discovery response message %1 from the Server %2: %3",
            nx::kit::utils::toString(discoveryResponseMessage), serverAddress, errorMessage);
        return;
    }

    for (const auto& cameraDiscoveryResponse: discoveryResponse.cameraDiscoveryResponses())
    {
        if (processedMacAddresses->contains(cameraDiscoveryResponse->macAddress()))
            continue;

        if (const auto resource = createTestCameraResource(
            cameraDiscoveryResponse->macAddress(),
            cameraDiscoveryResponse->videoLayoutSerialized(),
            discoveryResponse.mediaPort(),
            serverAddress))
        {
            processedMacAddresses->insert(cameraDiscoveryResponse->macAddress());
            resources->push_back(resource);
        }
    }
}

QnTestCameraResourcePtr QnTestCameraResourceSearcher::createTestCameraResource(
    const nx::utils::MacAddress& macAddress,
    const QString& videoLayoutString,
    int mediaPort,
    const QString& serverAddress) const
{
    const QnTestCameraResourcePtr resource(new QnTestCameraResource(serverModule()));

    const QString model = QLatin1String(QnTestCameraResource::kModel);

    const QnUuid resourceTypeId = qnResTypePool->getResourceTypeId(manufacturer(), model);
    if (resourceTypeId.isNull())
        return {};

    const nx::utils::Url url = nx::network::url::Builder()
        .setScheme("tcp")
        .setHost(serverAddress)
        .setPort(mediaPort)
        .setPath(macAddress.toString());

    resource->setTypeId(resourceTypeId);
    resource->setName(model);
    resource->setModel(model);
    resource->setMAC(macAddress);
    resource->setUrl(url.toString());

    const bool wasPropertyModified = resource->setProperty(
        ResourcePropertyKey::kVideoLayout, videoLayoutString);
    NX_ASSERT(!wasPropertyModified); //< The resource is not in the DB yet.

    const std::string videoLayoutLogMessage = videoLayoutString.isEmpty()
        ? ""
        : (" with video layout " + nx::kit::utils::toString(videoLayoutString));

    NX_INFO(this, "Found testcamera %1%2 with URL %3", resource, videoLayoutLogMessage, url);

    return resource;
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
        return QList<QnResourcePtr>(); //< Search if only host is present, not specific protocol.

    return QList<QnResourcePtr>();
}

#endif // defined(ENABLE_TEST_CAMERA)
