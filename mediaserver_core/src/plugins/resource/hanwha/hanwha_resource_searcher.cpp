#if defined(ENABLE_HANWHA)

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <nx/utils/log/log_main.h>

#include "hanwha_resource_searcher.h"
#include "hanwha_resource.h"
#include "hanwha_request_helper.h"

namespace
{
    static const QString kUpnpBasicDeviceType("Basic");
    static const QString kHanwhaManufacturerName("Hanwha Techwin");
    static const QString kHanwhaCameraName("Hanwha_Common");
}

namespace nx {
namespace mediaserver_core {
namespace plugins {

HanwhaResourceSearcher::HanwhaResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule)
{
	nx_upnp::DeviceSearcher::instance()->registerHandler(this, kUpnpBasicDeviceType);
}

QnResourcePtr HanwhaResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceType.isNull())
    {
        NX_WARNING(this, lm("No resource type for Hanwha camera. Id = %1").arg(resourceTypeId));
        return QnResourcePtr();
    }

    if (resourceType->getManufacture() != manufacture())
        return QnResourcePtr();

    QnNetworkResourcePtr result;
    result = QnVirtualCameraResourcePtr( new HanwhaResource() );
    result->setTypeId(resourceTypeId);
    return result;
}

QString HanwhaResourceSearcher::manufacture() const
{
    return kHanwhaManufacturerName;
}

QList<QnResourcePtr> HanwhaResourceSearcher::checkHostAddr(const utils::Url &url,
    const QAuthenticator& auth,
    bool isSearchAction)
{
    if (!url.scheme().isEmpty() && isSearchAction)
        return QList<QnResourcePtr>();

    auto rt = qnResTypePool->getResourceTypeByName(lit("Hanwha_Sunapi"));
    if (rt.isNull())
        return QList<QnResourcePtr>();

    QnResourceList result;
    HanwhaResourcePtr resource(new HanwhaResource());
    utils::Url urlCopy(url);
    urlCopy.setScheme("http");
    QString urlStr = urlCopy.toString();

    resource->setUrl(urlCopy.toString());
    resource->setDefaultAuth(auth);

    HanwhaRequestHelper helper(resource);
    HanwhaResponse systemInfo = helper.view("system/deviceinfo");
    if (!systemInfo.isSuccessful())
        return QList<QnResourcePtr>();

    auto macAddr = systemInfo.parameter<QString>("ConnectedMACAddress");
    auto model = systemInfo.parameter<QString>("Model");
    if (!macAddr || !model)
        return QList<QnResourcePtr>();
    auto firmware = systemInfo.parameter<QString>("FirmwareVersion");
    
    resource->setMAC(QnMacAddress(*macAddr));
    resource->setModel(*model);
    resource->setName(*model);
    if (firmware)
        resource->setFirmware(*firmware);
    resource->setTypeId(rt->getId());
    resource->setVendor(kHanwhaManufacturerName);
    result << resource;
    const int channel = resource->getChannel();
    if (isSearchAction)
        addMultichannelResources(result);
    else if (channel > 0 || getChannels(resource) > 1)
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
    if (!isHanwhaCamera(devInfo))
        return false;

    QnMacAddress cameraMac(devInfo.udn.split(L'-').last());
    if (cameraMac.isNull())
        cameraMac = QnMacAddress(devInfo.serialNumber);

    QString model(devInfo.modelName);

	{
		QnMutexLocker lock(&m_mutex);
		if (m_alreadFoundMacAddresses.find(cameraMac.toString()) != m_alreadFoundMacAddresses.end())
            return true;
    }

    decltype(m_foundUpnpResources) foundUpnpResources;

    QAuthenticator defaultAuth;
    defaultAuth.setUser("admin");
    defaultAuth.setPassword("4321");
    createResource(devInfo, cameraMac, defaultAuth, foundUpnpResources);

    QnMutexLocker lock(&m_mutex);
    m_alreadFoundMacAddresses.insert(cameraMac.toString());
    m_foundUpnpResources += foundUpnpResources;

	return true;
}

void HanwhaResourceSearcher::createResource(
    const nx_upnp::DeviceInfo& devInfo,
    const QnMacAddress& mac,
    const QAuthenticator& auth,
    QnResourceList& result)
{

    auto rt = qnResTypePool->getResourceTypeByName(lit("Hanwha_Sunapi"));
    if (rt.isNull())
        return;

    QnResourceData resourceData = qnStaticCommon->dataPool()->data(devInfo.manufacturer, devInfo.modelName);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return;

    HanwhaResourcePtr resource( new HanwhaResource() );

    resource->setTypeId(rt->getId());
    resource->setVendor(devInfo.manufacturer);
    resource->setName(devInfo.modelName);
    resource->setModel(devInfo.modelName);
    resource->setUrl(devInfo.presentationUrl);
    resource->setMAC(mac);

    if (!auth.isNull()) 
        resource->setDefaultAuth(auth);

    result << resource;
    addMultichannelResources(result);
}

int HanwhaResourceSearcher::getChannels(const HanwhaResourcePtr& resource)
{
    auto result = m_channelsByCamera.value(resource->getUniqueId());
    if (result > 0)
        return result;
    

    HanwhaRequestHelper helper(resource);
    auto attributes = helper.fetchAttributes(lit("attributes/System"));
    const auto maxChannels = attributes.attribute<int>(lit("System/MaxChannel"));
    if (maxChannels)
        result = *maxChannels;

    m_channelsByCamera.insert(resource->getUniqueId(), result);
    return result;
}

template <typename T>
void HanwhaResourceSearcher::addMultichannelResources(QList<T>& result)
{
    HanwhaResourcePtr firstResource = result.first().template dynamicCast<HanwhaResource>();

    const auto channels = getChannels(firstResource);
    if (channels > 1)
    {
        firstResource->updateToChannel(0);
        for (int i = 1; i < channels; ++i)
        {
            HanwhaResourcePtr resource(new HanwhaResource());

            auto rt = qnResTypePool->getResourceTypeByName(lit("Hanwha_Sunapi"));
            if (rt.isNull())
                return;

            resource->setTypeId(rt->getId());
            resource->setVendor(kHanwhaManufacturerName);
            resource->setName(firstResource->getName());
            resource->setModel(firstResource->getName());
            resource->setMAC(firstResource->getMAC());

            auto auth = firstResource->getAuth();
            if (!auth.isNull())
                resource->setDefaultAuth(auth);

            resource->setUrl(firstResource->getUrl());
            resource->updateToChannel(i);

            result.push_back(resource);
        }
    }
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
