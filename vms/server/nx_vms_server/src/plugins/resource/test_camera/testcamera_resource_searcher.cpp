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
    for(const DiscoverySocket& info: m_discoverySockets)
        delete info.socket;
    m_discoverySockets.clear();
}

bool QnTestCameraResourceSearcher::updateSocketList()
{
    const qint64 currentTime = getUsecTimer();
    if (currentTime - m_socketUpdateTime <= SOCK_UPDATE_INTERVAL)
        return false;

    clearSocketList();
    for (const auto& address: nx::network::allLocalAddresses(nx::network::ipV4))
    {
        DiscoverySocket info(nx::network::SocketFactory::createDatagramSocket().release(),
            QHostAddress(address.toString()));
        if (info.socket->bind(address.toString(), 0))
            m_discoverySockets << info;
        else
            delete info.socket;
    }
    m_socketUpdateTime = currentTime;
    return true;
}

void QnTestCameraResourceSearcher::sendBroadcast()
{
    ini().reload();

    const QByteArray testCameraFindMessage = ini().discoveryMessage + QByteArray("\n");

    NX_VERBOSE(this, "Broadcasting discovery messages to %1 sockets.", m_discoverySockets.size());

    for (const DiscoverySocket& info: m_discoverySockets)
    {
        info.socket->sendTo(
            testCameraFindMessage.constData(), testCameraFindMessage.size(),
            nx::network::BROADCAST_ADDRESS, ini().discoveryPort);
    }
}

QnResourceList QnTestCameraResourceSearcher::findResources()
{
    NX_VERBOSE(this, "Reading discovery responses from %2 sockets.", m_discoverySockets.size());

    if (updateSocketList())
    {
        sendBroadcast();
        QnSleep::msleep(1000);
    }

    QnResourceList resources;
    std::set<nx::utils::MacAddress> processedMacAddresses;

    for (const DiscoverySocket& info: m_discoverySockets)
    {
        nx::network::AbstractDatagramSocket* const socket = info.socket;
        while (socket->hasData())
        {
            NX_VERBOSE(this, "Reading discovery response from %1.", socket->getForeignHostName());
 
            QByteArray discoveryResponseMessage;
            discoveryResponseMessage.resize(
                nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE);

            nx::network::SocketAddress remoteEndpoint;
            const int bytesRead = socket->recvFrom(
                discoveryResponseMessage.data(), discoveryResponseMessage.size(), &remoteEndpoint);
            const QString testcameraHost = remoteEndpoint.address.toString();
            if (bytesRead < 1)
            {
                NX_DEBUG(this, "Unable to read discovery response from testcamera %1: code %2",
                    testcameraHost, bytesRead);
                continue;
            }

            discoveryResponseMessage.resize(bytesRead);

            processDiscoveryResponseMessage(
                discoveryResponseMessage,
                testcameraHost,
                &resources,
                &processedMacAddresses);
        }
    }

    sendBroadcast();

    NX_VERBOSE(this, "Finished testcamera discovery: created %1 resource(s).", resources.size());
    return resources;
}

void QnTestCameraResourceSearcher::processDiscoveryResponseMessage(
    const QByteArray& discoveryResponseMessage,
    const QString& testcameraHost,
    QnResourceList* resources,
    std::set<nx::utils::MacAddress>* processedMacAddresses) const
{
    QString errorMessage;
    const nx::vms::testcamera::DiscoveryResponse discoveryResponse(
        discoveryResponseMessage, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        NX_DEBUG(this,
            "Ignoring received testcamera discovery response %1 from testcamera %2: %3",
            nx::kit::utils::toString(discoveryResponseMessage), testcameraHost, errorMessage);
        return;
    }

    NX_DEBUG(this, "Received discovery response from testcamera %1 hosting %2 Cameras: %3",
        testcameraHost,
        discoveryResponse.cameraDiscoveryResponses().size(),
        nx::kit::utils::toString(discoveryResponseMessage));

    for (const auto& cameraDiscoveryResponse: discoveryResponse.cameraDiscoveryResponses())
    {
        if (processedMacAddresses->count(cameraDiscoveryResponse->macAddress()) > 0)
        {
            NX_DEBUG(this, "Ignoring Camera with already processed MAC %1.",
                cameraDiscoveryResponse->macAddress());
            continue;
        }

        if (const auto resource = createDiscoveredTestCameraResource(
            cameraDiscoveryResponse->macAddress(),
            cameraDiscoveryResponse->videoLayoutSerialized(),
            discoveryResponse.mediaPort(),
            testcameraHost))
        {
            processedMacAddresses->insert(cameraDiscoveryResponse->macAddress());
            resources->push_back(resource);
        }
    }
}

QnTestCameraResourcePtr QnTestCameraResourceSearcher::createDiscoveredTestCameraResource(
    const nx::utils::MacAddress& macAddress,
    const QString& videoLayoutString,
    int mediaPort,
    const QString& testcameraHost) const
{
    const QnTestCameraResourcePtr resource(new QnTestCameraResource(serverModule()));

    const QString model = QLatin1String(QnTestCameraResource::kModel);

    const QnUuid resourceTypeId = qnResTypePool->getResourceTypeId(manufacturer(), model);
    if (resourceTypeId.isNull())
    {
        NX_DEBUG(this, "Missing Resource type for manifacturer %1, model %2.",
            nx::kit::utils::toString(manufacturer()), nx::kit::utils::toString(model));
        return {};
    }

    const nx::utils::Url url = nx::network::url::Builder()
        .setScheme("tcp")
        .setHost(testcameraHost)
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
