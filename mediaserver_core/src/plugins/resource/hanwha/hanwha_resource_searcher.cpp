#if defined(ENABLE_HANWHA)

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/scope_guard.h>

#include "hanwha_resource_searcher.h"
#include "hanwha_resource.h"
#include "hanwha_request_helper.h"
#include "hanwha_common.h"
#include <media_server/media_server_module.h>
#include <nx/mediaserver/resource/shared_context_pool.h>

namespace {

static const QString kUpnpBasicDeviceType("Basic");
static const QString kHanwhaCameraName("Hanwha_Common");
static const QString kHanwhaResourceTypeName = lit("Hanwha_Sunapi");
static const QString kHanwhaDefaultUser = lit("admin");
static const QString kHanwhaDefaultPassword = lit("4321");

} // namespace

namespace nx {
namespace mediaserver_core {
namespace plugins {

HanwhaResult<HanwhaInformation> HanwhaResourceSearcher::cachedDeviceInfo(const QAuthenticator& auth, const QUrl& url)
{
    // This is not the same context as for resources, bc we do not have MAC address before hand.
    auto sharedId = lit("hash_%1:%2").arg(url.host()).arg(url.port(80));
    const auto context = qnServerModule->sharedContextPool()
        ->sharedContext<HanwhaSharedResourceContext>(sharedId);

    context->setRecourceAccess(url, auth);
    m_sharedContext[sharedId] = context;
    return context->information();
}

HanwhaResourceSearcher::HanwhaResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule),
    nx_upnp::SearchAutoHandler(kUpnpBasicDeviceType)
{
}

QnResourcePtr HanwhaResourceSearcher::createResource(
    const QnUuid &resourceTypeId,
    const QnResourceParams& /*params*/)
{
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    NX_EXPECT(!resourceType.isNull());
    if (resourceType.isNull())
    {
        NX_WARNING(this, lm("No resource type for Hanwha camera. Id = %1").arg(resourceTypeId));
        return QnResourcePtr();
    }

    if (resourceType->getManufacture() != manufacture())
        return QnResourcePtr();

    QnNetworkResourcePtr result;
    result = QnVirtualCameraResourcePtr(new HanwhaResource());
    result->setTypeId(resourceTypeId);
    return result;
}

QString HanwhaResourceSearcher::manufacture() const
{
    return kHanwhaManufacturerName;
}

QList<QnResourcePtr> HanwhaResourceSearcher::checkHostAddr(
    const QUrl& url,
    const QAuthenticator& auth,
    bool isSearchAction)
{
    if (!url.scheme().isEmpty() && isSearchAction)
        return QList<QnResourcePtr>();

    auto rt = qnResTypePool->getResourceTypeByName(kHanwhaResourceTypeName);
    if (rt.isNull())
        return QList<QnResourcePtr>();

    QnResourceList result;
    HanwhaResourcePtr resource(new HanwhaResource());
    QUrl urlCopy(url);
    urlCopy.setScheme("http");

    resource->setUrl(urlCopy.toString());
    resource->setDefaultAuth(auth);

    const auto info = cachedDeviceInfo(auth, urlCopy);
    if (!info || info->macAddress.isEmpty())
        return QList<QnResourcePtr>();

    resource->setMAC(QnMacAddress(info->macAddress));
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
    return result;
}

QnResourceList HanwhaResourceSearcher::findResources(void)
{
	QnResourceList upnpResults;

	{
		QnMutexLocker lock(&m_mutex);
		upnpResults = m_foundUpnpResources;
		m_foundUpnpResources.clear();
		m_alreadFoundMacAddresses.clear();
	}

    return upnpResults;
}

bool HanwhaResourceSearcher::isHanwhaCamera(const nx_upnp::DeviceInfo& devInfo) const
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
    const SocketAddress& deviceEndpoint,
    const nx_upnp::DeviceInfo& devInfo,
    const QByteArray& /*xmlDevInfo*/)
{
    if (discoveryMode() == DiscoveryMode::disabled)
        return false;

    if (!isHanwhaCamera(devInfo))
        return false;

    QnMacAddress cameraMac(devInfo.udn.split(L'-').last());
    if (cameraMac.isNull())
        cameraMac = QnMacAddress(devInfo.serialNumber);
    if (cameraMac.isNull())
    {
        NX_WARNING(this, lm("Can't obtain MAC address for hanwha device. udn=%1. serial=%2.")
            .arg(devInfo.udn).arg(devInfo.serialNumber));
        return false;
    }

    QString model(devInfo.modelName);

	{
		QnMutexLocker lock(&m_mutex);
        const bool alreadyFound = m_alreadFoundMacAddresses.find(cameraMac.toString())
            != m_alreadFoundMacAddresses.end();

		if (alreadyFound)
            return true;
    }

    decltype(m_foundUpnpResources) foundUpnpResources;

    createResource(devInfo, cameraMac, foundUpnpResources);

    QnMutexLocker lock(&m_mutex);
    m_alreadFoundMacAddresses.insert(cameraMac.toString());
    m_foundUpnpResources += foundUpnpResources;

	return true;
}

void HanwhaResourceSearcher::createResource(
    const nx_upnp::DeviceInfo& devInfo,
    const QnMacAddress& mac,
    QnResourceList& result)
{
    auto rt = qnResTypePool->getResourceTypeByName(kHanwhaResourceTypeName);
    if (rt.isNull())
        return;

    QnResourceData resourceData = qnStaticCommon
        ->dataPool()
        ->data(devInfo.manufacturer, devInfo.modelName);

    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return;

    HanwhaResourcePtr resource( new HanwhaResource() );

    resource->setTypeId(rt->getId());
    resource->setVendor(kHanwhaManufacturerName);
    resource->setName(devInfo.modelName);
    resource->setModel(devInfo.modelName);
    resource->setUrl(devInfo.presentationUrl);
    resource->setMAC(mac);

    auto resPool = commonModule()->resourcePool();
    auto rpRes = resPool->getNetResourceByPhysicalId(
        resource->getUniqueId()).dynamicCast<HanwhaResource>();

    auto auth = rpRes ? rpRes->getAuth() : getDefaultAuth();
    resource->setDefaultAuth(auth);
    result << resource;

    addMultichannelResources(result, auth);
}

template <typename T>
void HanwhaResourceSearcher::addMultichannelResources(QList<T>& result, const QAuthenticator& auth)
{
    HanwhaResourcePtr firstResource = result.first().template dynamicCast<HanwhaResource>();

    const auto info = cachedDeviceInfo(auth, firstResource->getUrl());
    if (!info)
        return;

    const auto channels = info->channelCount;
    if (channels > 1)
    {
        firstResource->updateToChannel(0);
        for (int i = 1; i < channels; ++i)
        {
            HanwhaResourcePtr resource(new HanwhaResource());

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
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
