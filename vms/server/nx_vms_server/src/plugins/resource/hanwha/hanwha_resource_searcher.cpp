#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <common/common_module.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/scope_guard.h>
#include <nx/fusion/serialization/lexical.h>

#include "hanwha_resource_searcher.h"
#include "hanwha_resource.h"
#include "hanwha_request_helper.h"
#include "hanwha_common.h"
#include "vms_server_hanwha_ini.h"

#include <media_server/media_server_module.h>
#include <nx/vms/server/resource/shared_context_pool.h>

namespace {

static const QString kUpnpBasicDeviceType("Basic");
static const QString kHanwhaCameraName("Hanwha_Common");
static const QString kHanwhaResourceTypeName = lit("Hanwha_Sunapi");
static const QString kHanwhaDefaultUser = lit("admin");
static const QString kHanwhaDefaultPassword = lit("4321");

static const int kSunApiProbeSrcPort = 7711;
static const int kSunApiProbeDstPort = 7701;
static std::chrono::seconds kSunapiSocketSendTimeout(5);

static const int64_t kSunApiDiscoveryTimeoutMs = 10 * 60 * 1000; //< 10 minutes.

} // namespace

namespace nx {
namespace vms::server {
namespace plugins {

HanwhaResourceSearcher::HanwhaResourceSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    nx::network::upnp::SearchAutoHandler(serverModule->upnpDeviceSearcher(), kUpnpBasicDeviceType),
    vms::server::ServerModuleAware(serverModule),
    m_sunapiProbePackets(createProbePackets())
{
    ini().reload();
}

HanwhaResult<HanwhaInformation> HanwhaResourceSearcher::cachedDeviceInfo(const QAuthenticator& auth, const nx::utils::Url& url)
{
    // This is not the same context as for resources, bc we do not have MAC address before hand.
    auto sharedId = lit("hash_%1:%2").arg(url.host()).arg(url.port(80));
    const auto context = serverModule()->sharedContextPool()
        ->sharedContext<HanwhaSharedResourceContext>(sharedId);

    context->setResourceAccess(url, auth);
    return context->information();
}

std::vector<std::vector<quint8>> HanwhaResourceSearcher::createProbePackets()
{
    std::vector<std::vector<quint8>> result;
    result.resize(2);
    result[0].resize(262);
    result[0][0] = '\x1';

    result[1].resize(334);
    result[1][0] = '\x6';

    return result;
}

QnResourcePtr HanwhaResourceSearcher::createResource(
    const QnUuid &resourceTypeId,
    const QnResourceParams& /*params*/)
{
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    NX_ASSERT(!resourceType.isNull());
    if (resourceType.isNull())
    {
        NX_WARNING(this, lm("No resource type for Hanwha camera. Id = %1").arg(resourceTypeId));
        return QnResourcePtr();
    }

    if (resourceType->getManufacturer() != manufacturer())
        return QnResourcePtr();

    QnNetworkResourcePtr result;
    result = QnVirtualCameraResourcePtr(new HanwhaResource(serverModule()));
    result->setTypeId(resourceTypeId);
    return result;
}

QString HanwhaResourceSearcher::manufacturer() const
{
    return kHanwhaManufacturerName;
}

QList<QnResourcePtr> HanwhaResourceSearcher::checkHostAddr(const utils::Url &url,
    const QAuthenticator& auth,
    bool isSearchAction)
{
    if (!url.scheme().isEmpty() && isSearchAction)
        return QList<QnResourcePtr>();

    auto rt = qnResTypePool->getResourceTypeByName(kHanwhaResourceTypeName);
    if (rt.isNull())
        return QList<QnResourcePtr>();

    QnResourceList result;
    HanwhaResourcePtr resource(new HanwhaResource(serverModule()));
    utils::Url urlCopy(url);
    urlCopy.setScheme("http");

    resource->setUrl(urlCopy.toString());
    resource->setDefaultAuth(auth);

    const auto info = cachedDeviceInfo(auth, urlCopy);
    if (!info || info->macAddress.isEmpty())
        return QList<QnResourcePtr>();

    resource->setMAC(nx::utils::MacAddress(info->macAddress));
    resource->setModel(info->model);
    resource->setName(info->model);
    resource->setFirmware(info->firmware);
    resource->setTypeId(rt->getId());
    resource->setVendor(kHanwhaManufacturerName);
    result << resource;
    const int channel = resource->getChannel();
    if (isSearchAction)
        addMultichannelResources(result, auth);
    else if (channel > 0 || info->channelCount > 1)
        resource->updateToChannel(channel);

    resource->setDeviceType(fromHanwhaToNxDeviceType(info->deviceType));

    return result;
}

QnResourceList HanwhaResourceSearcher::findResources()
{
    QnResourceList upnpResults;

    {
        QnMutexLocker lock(&m_mutex);
        upnpResults = m_foundUpnpResources;
        m_foundUpnpResources.clear();
        m_alreadyFoundMacAddresses.clear();
    }

    NX_VERBOSE(this, "Found UPnP resources: %1", upnpResults);
    addResourcesViaSunApi(upnpResults);
    NX_VERBOSE(this, "Found UPnP or SUNAPI resources: %1", upnpResults);
    return upnpResults;
}

void HanwhaResourceSearcher::addResourcesViaSunApi(QnResourceList& upnpResults)
{
    sendSunApiProbe();
    readSunApiResponse(upnpResults);
}

void HanwhaResourceSearcher::updateSocketList()
{
    if (!m_sunapiReceiveSocket)
    {
        auto socket = nx::network::SocketFactory::createDatagramSocket();
        if (socket->setReuseAddrFlag(true) &&
            socket->bind(network::BROADCAST_ADDRESS, kSunApiProbeSrcPort))
        {
            m_sunapiReceiveSocket = std::move(socket);
        }
    }

    const auto interfaceList = nx::network::getAllIPv4Interfaces();
    if (m_lastInterfaceList == interfaceList)
        return;

    m_lastInterfaceList = interfaceList;
    m_sunApiSocketList.clear();
    for (const nx::network::QnInterfaceAndAddr& iface: interfaceList)
    {
        auto socket(nx::network::SocketFactory::createDatagramSocket());
        if (!socket->setReuseAddrFlag(true) ||
            !socket->bind(iface.address.toString(), kSunApiProbeSrcPort) ||
            !socket->setSendTimeout(kSunapiSocketSendTimeout))
        {
            continue;
        }
        m_sunApiSocketList.push_back(std::move(socket));
    }
}

bool HanwhaResourceSearcher::isHostBelongsToValidSubnet(const QHostAddress& address) const
{
    using namespace nx::network;
    const auto interfaceList = getAllIPv4Interfaces(
        InterfaceListPolicy::keepAllAddressesPerInterface);
    return std::any_of(
        interfaceList.begin(), interfaceList.end(),
        [&address](const nx::network::QnInterfaceAndAddr& netInterface)
        {
            return netInterface.isHostBelongToIpv4Network(address);
        });
}

bool HanwhaResourceSearcher::parseSunApiData(const QByteArray& data, SunApiData* outData)
{
    // Packet format is reverse engenered.
    bool isPacketV1 = data.size() == 262 && data[0] == '\x0b';
    bool isPacketV2 = data.size() == 334 && data[0] == '\x0c';
    if (!isPacketV1 && !isPacketV2)
        return false;

    static const int kMacAddressOffset = 19;
    outData->macAddress = nx::utils::MacAddress(QLatin1String(data.data() + kMacAddressOffset));
    if (outData->macAddress.isNull())
        return false;

    static const int kModelOffset = 109;
    outData->modelName = QLatin1String(data.data() + kModelOffset);

    static const int kUrlOffset = 133;
    const auto urlStr = QLatin1String(data.data() + kUrlOffset);
    QUrl url(urlStr);
    if (!url.isValid() || !isHostBelongsToValidSubnet(QHostAddress(url.host())))
        return false;
    outData->presentationUrl = url.toString(QUrl::RemovePath);
    outData->manufacturer = kHanwhaManufacturerName;

    return true;
}

void HanwhaResourceSearcher::sendSunApiProbe()
{
    updateSocketList();
    for (const auto& socket: m_sunApiSocketList)
    {
        for (const auto& packet : m_sunapiProbePackets)
        {
            socket->sendTo(packet.data(), (int)packet.size(),
                nx::network::BROADCAST_ADDRESS, kSunApiProbeDstPort);
        }
    }
}

void HanwhaResourceSearcher::readSunApiResponse(QnResourceList& resultResourceList)
{
    bool success = false;
    for (const auto& socket: m_sunApiSocketList)
    {
        success = readSunApiResponseFromSocket(socket.get(), &resultResourceList);
        if (!success)
            m_sunApiSocketList.clear();
    }

    if (m_sunapiReceiveSocket && m_sunapiReceiveSocket->hasData())
    {
        success = readSunApiResponseFromSocket(m_sunapiReceiveSocket.get(), &resultResourceList);
        if (!success)
            m_sunapiReceiveSocket.reset();
    }
}

bool HanwhaResourceSearcher::readSunApiResponseFromSocket(
    network::AbstractDatagramSocket* socket,
    QnResourceList* resultResourceList)
{
    NX_ASSERT(socket);
    if (!socket)
        return false;

    auto resourceAlreadyFound =
        [](const QnResourceList* resultResourceList, const nx::utils::MacAddress& macAddress)
        {
            return std::any_of(
                resultResourceList->cbegin(), resultResourceList->cend(),
                [&macAddress](const QnResourcePtr& resource)
                {
                    return nx::utils::MacAddress(resource->getUniqueId()) == macAddress;
                });
        };

    while(socket->hasData())
    {
        nx::Buffer datagram;
        datagram.resize(nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE);
        const auto bytesRead = socket->recv(datagram.data(), datagram.size());
        if (bytesRead < 1)
            return false;

        SunApiData sunApiData;
        if (parseSunApiData(datagram.left(bytesRead), &sunApiData))
        {
            if (!resourceAlreadyFound(resultResourceList, sunApiData.macAddress))
                createResource(sunApiData, sunApiData.macAddress, *resultResourceList);

            QnMutexLocker lock(&m_mutex);
            m_sunapiDiscoveredDevices[sunApiData.macAddress] = sunApiData;
        }
    }
    return true;
}

bool HanwhaResourceSearcher::isHanwhaCamera(const nx::network::upnp::DeviceInfo& devInfo) const
{
    auto simplifyStr = [](const QString& value)
    {
        return value.toLower().trimmed().replace(lit(" "), lit(""));
    };

    const auto vendor = simplifyStr(devInfo.manufacturer);
    if (vendor.startsWith(lit("hanwha")))
        return true;

    static const QString kSamsung(lit("samsungtechwin"));
    return vendor.contains(kSamsung) || simplifyStr(devInfo.manufacturerUrl).contains(kSamsung);
}

bool HanwhaResourceSearcher::processPacket(
    const QHostAddress& /*discoveryAddr*/,
    const nx::network::SocketAddress& deviceEndpoint,
    const nx::network::upnp::DeviceInfo& devInfo,
    const QByteArray& /*xmlDevInfo*/)
{
    if (discoveryMode() == DiscoveryMode::disabled)
        return false;

    if (!isHanwhaCamera(devInfo))
        return false;

    NX_VERBOSE(this, "Got UPnP Hanwha device: [%1] (serial: [%2], address: [%3])",
        devInfo.modelName, devInfo.serialNumber, deviceEndpoint);
    nx::utils::MacAddress cameraMac(devInfo.udn.split(L'-').last());
    if (cameraMac.isNull())
        cameraMac = nx::utils::MacAddress(devInfo.serialNumber);
    if (cameraMac.isNull())
    {
        NX_WARNING(this,
            "Can't obtain MAC address for hanwha device: [%1] (udn: [%2], serial: [%3])",
            devInfo.modelName, devInfo.udn, devInfo.serialNumber);
        return false;
    }

    {
        QnMutexLocker lock(&m_mutex);

        // Due to some bugs in UPnP implementation higher priority is given
        // to the native SUNAPI discovery protocol.
        auto itr = m_sunapiDiscoveredDevices.find(cameraMac);
        if (itr != m_sunapiDiscoveredDevices.end()
            && !itr->timer.hasExpired(kSunApiDiscoveryTimeoutMs))
        {
            NX_DEBUG(this,
                "Device [%1] (%2) was discovered by SUNAPI, skipping device info found by UPnP",
                itr.value().modelName, itr.key());
            return false;
        }

        const bool alreadyFound = m_alreadyFoundMacAddresses.find(cameraMac.toString())
            != m_alreadyFoundMacAddresses.end();

        if (alreadyFound)
            return true;
    }

    decltype(m_foundUpnpResources) foundUpnpResources;

    createResource(devInfo, cameraMac, foundUpnpResources);

    QnMutexLocker lock(&m_mutex);
    m_alreadyFoundMacAddresses.insert(cameraMac.toString());
    m_foundUpnpResources += foundUpnpResources;

    return true;
}

bool HanwhaResourceSearcher::isEnabled() const
{
    return discoveryMode() != DiscoveryMode::disabled;
}

void HanwhaResourceSearcher::createResource(
    const nx::network::upnp::DeviceInfo& devInfo,
    const nx::utils::MacAddress& mac,
    QnResourceList& result)
{
    auto rt = qnResTypePool->getResourceTypeByName(kHanwhaResourceTypeName);
    if (rt.isNull())
        return;

    QnResourceData resourceData = commonModule()->resourceDataPool()
        ->data(devInfo.manufacturer, devInfo.modelName);

    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
        return;

    HanwhaResourcePtr resource(new HanwhaResource(serverModule()));

    resource->setTypeId(rt->getId());
    resource->setVendor(kHanwhaManufacturerName);
    resource->setName(devInfo.modelName);
    resource->setModel(devInfo.modelName);

    QUrl url(devInfo.presentationUrl);
    if (url.port() == -1)
        url.setPort(nx::network::http::DEFAULT_HTTP_PORT);

    resource->setUrl(url.toString(QUrl::RemovePath));
    resource->setMAC(mac);

    auto resPool = serverModule()->resourcePool();
    auto rpRes = resPool->getNetResourceByPhysicalId(
        resource->getUniqueId()).dynamicCast<HanwhaResource>();

    auto auth = rpRes ? rpRes->getAuth() : getDefaultAuth();
    resource->setDefaultAuth(auth);

    NX_DEBUG(this, "Found resource: %1", resource);
    result << resource;
    if (rpRes)
        addMultichannelResources(result, auth);
}

template <typename T>
void HanwhaResourceSearcher::addMultichannelResources(QList<T>& result, const QAuthenticator& auth)
{
    if (shouldStop())
        return;

    const auto firstResource = result.last().template dynamicCast<HanwhaResource>();
    const auto physicalId = firstResource->getPhysicalId();

    auto& baseDeviceInfo = m_baseDeviceInfos[physicalId];
    if (!baseDeviceInfo.isValid())
    {
        const auto deviceInfo = cachedDeviceInfo(auth, firstResource->getUrl());
        baseDeviceInfo.numberOfChannels = deviceInfo->channelCount;
        baseDeviceInfo.deviceType = fromHanwhaToNxDeviceType(deviceInfo->deviceType);
    }

    if (baseDeviceInfo.isValid() && baseDeviceInfo.numberOfChannels > 1)
    {
        firstResource->setDeviceType(baseDeviceInfo.deviceType);
        firstResource->updateToChannel(0);

        for (int i = 1; i < baseDeviceInfo.numberOfChannels; ++i)
        {
            HanwhaResourcePtr resource(new HanwhaResource(serverModule()));

            auto rt = qnResTypePool->getResourceTypeByName(kHanwhaResourceTypeName);
            if (rt.isNull())
                return;

            resource->setTypeId(rt->getId());
            resource->setVendor(kHanwhaManufacturerName);
            resource->setName(firstResource->getName());
            resource->setModel(firstResource->getName());
            resource->setMAC(firstResource->getMAC());

            resource->setDefaultAuth(auth);

            resource->setUrl(firstResource->getUrl());
            resource->updateToChannel(i);
            resource->setDeviceType(baseDeviceInfo.deviceType);

            result.push_back(resource);
        }
    }
}

QAuthenticator HanwhaResourceSearcher::getDefaultAuth()
{
    QAuthenticator defaultAuth;
    defaultAuth.setUser(kHanwhaDefaultUser);
    defaultAuth.setPassword(kHanwhaDefaultPassword);
    return defaultAuth;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
