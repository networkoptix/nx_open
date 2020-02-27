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

using namespace std::chrono_literals;

static const qint64 SOCK_UPDATE_INTERVAL = 1000000ll * 60 * 5;
static const QString kDiscoveryPortParameterName = "discoveryPort";

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
}

/**
 * @return Whether the socket list has been recreated.
 */
bool QnTestCameraResourceSearcher::updateSocketListIfNeeded()
{
    const qint64 currentTime = getUsecTimer();
    if (currentTime - m_socketUpdateTime <= SOCK_UPDATE_INTERVAL)
        return false;

    m_discoverySockets.clear();
    for (const auto& address: nx::network::allLocalAddresses(nx::network::ipV4))
    {
        auto socket = nx::network::SocketFactory::createDatagramSocket();
        if (socket->bind(address.toString(), 0))
            m_discoverySockets.push_back(std::move(socket));
    }
    m_socketUpdateTime = currentTime;
    return true;
}

void QnTestCameraResourceSearcher::sendDiscoveryMessage(
    nx::network::AbstractDatagramSocket* socket, const QString& addr, int port) const
{
    const QByteArray discoveryMessage = ini().discoveryMessage + QByteArray("\n");

    NX_VERBOSE(this, "Sending discovery message to %1:%2.", addr, port);

    if (!socket->sendTo(discoveryMessage.constData(), discoveryMessage.size(), addr, port))
        NX_VERBOSE(this, "Failed sending discovery message to %1:%2.", addr, port);
}

void QnTestCameraResourceSearcher::sendBroadcast()
{
    ini().reload();

    NX_VERBOSE(this, "Broadcasting discovery messages to %1 sockets.", m_discoverySockets.size());

    for (const auto& socket: m_discoverySockets)
        sendDiscoveryMessage(socket.get(), nx::network::BROADCAST_ADDRESS, ini().discoveryPort);
}

bool QnTestCameraResourceSearcher::readDiscoveryResponse(
    nx::network::AbstractDatagramSocket* socket,
    QnResourceList* resources,
    std::set<nx::utils::MacAddress>* processedMacAddresses) const
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
        NX_DEBUG(this, "Unable to read discovery response from testcamera %1: code %2.",
            testcameraHost, bytesRead);
        return false;
    }

    discoveryResponseMessage.resize(bytesRead);

    processDiscoveryResponseMessage(
        discoveryResponseMessage,
        remoteEndpoint,
        resources,
        processedMacAddresses);

    return true;
}

QnResourceList QnTestCameraResourceSearcher::findResources()
{
    NX_VERBOSE(this, "Reading discovery responses from %2 sockets.", m_discoverySockets.size());

    if (updateSocketListIfNeeded())
    {
        sendBroadcast();
        QnSleep::msleep(1000);
    }

    QnResourceList resources;
    std::set<nx::utils::MacAddress> processedMacAddresses;

    for (const auto& socket: m_discoverySockets)
    {
        while (socket->hasData())
        {
            if (!readDiscoveryResponse(socket.get(), &resources, &processedMacAddresses))
                break;
        }
    }

    sendBroadcast();

    NX_VERBOSE(this, "Finished testcamera discovery: created %1 resource(s).", resources.size());
    return resources;
}

void QnTestCameraResourceSearcher::processDiscoveryResponseMessage(
    const QByteArray& discoveryResponseMessage,
    const nx::network::SocketAddress& testcameraDiscoverySocketAddress,
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
            nx::kit::utils::toString(discoveryResponseMessage),
            testcameraDiscoverySocketAddress,
            errorMessage);
        return;
    }

    NX_DEBUG(this, "Received discovery response from testcamera %1 hosting %2 Cameras: %3",
        testcameraDiscoverySocketAddress,
        discoveryResponse.cameraDiscoveryResponses().size(),
        nx::kit::utils::toString(discoveryResponseMessage));

    for (const auto& cameraDiscoveryResponse: discoveryResponse.cameraDiscoveryResponses())
    {
        if (processedMacAddresses->count(cameraDiscoveryResponse->macAddress()) > 0)
        {
            NX_DEBUG(this, "Ignoring testcamera with already processed MAC %1.",
                cameraDiscoveryResponse->macAddress());
            continue;
        }

        if (const auto resource = createDiscoveredTestCameraResource(
            cameraDiscoveryResponse->macAddress(),
            cameraDiscoveryResponse->videoLayoutSerialized(),
            discoveryResponse.mediaPort(),
            testcameraDiscoverySocketAddress))
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
    const nx::network::SocketAddress& testcameraDiscoverySocketAddress) const
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

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(
        kDiscoveryPortParameterName,
        QString::number(testcameraDiscoverySocketAddress.port));

    const nx::utils::Url url = nx::network::url::Builder()
        .setScheme("tcp")
        .setHost(testcameraDiscoverySocketAddress.address.toString())
        .setPort(mediaPort)
        .setPath(macAddress.toString())
        .setQuery(urlQuery);

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

    NX_DEBUG(this, "Discovered testcamera %1%2 with URL %3", resource, videoLayoutLogMessage, url);

    return resource;
}

QnResourcePtr QnTestCameraResourceSearcher::createResource(
    const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    const QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        NX_WARNING(this, "No resource type for id: %1", resourceTypeId);
        return result;
    }

    if (resourceType->getManufacturer() != manufacturer())
    {
        NX_VERBOSE(this, "Manufacturer %1 != %2", resourceType->getManufacturer(), manufacturer());
        return result;
    }

    result = QnVirtualCameraResourcePtr(new QnTestCameraResource(serverModule()));
    result->setTypeId(resourceTypeId);

    NX_DEBUG(this, "Created testcamera resource %1, typeId %2.", result, resourceTypeId);
    return result;
}

QString QnTestCameraResourceSearcher::manufacturer() const
{
    return QLatin1String(QnTestCameraResource::kManufacturer);
}

QList<QnResourcePtr> QnTestCameraResourceSearcher::checkHostAddr(const nx::utils::Url& url,
    const QAuthenticator& /*authenticator*/, bool isSearchAction)
{
    NX_VERBOSE(this, "%1(%2, authenticator, isSearchAction: %3)",
        __func__, nx::kit::utils::toString(url.toStdString()), isSearchAction ? "true" : "false");

    int discoveryPort = QUrlQuery(url.query())
        .queryItemValue(kDiscoveryPortParameterName).toInt();

    if (discoveryPort == 0)
        discoveryPort = url.port(ini().discoveryPort);

    const auto socket = nx::network::SocketFactory::createDatagramSocket();

    sendDiscoveryMessage(socket.get(), url.host(), discoveryPort);

    socket->setRecvTimeout(1s);

    QnResourceList resources;
    std::set<nx::utils::MacAddress> processedMacAddresses;

    if (!readDiscoveryResponse(socket.get(), &resources, &processedMacAddresses))
        return {};

    return resources;
}

#endif // defined(ENABLE_TEST_CAMERA)
