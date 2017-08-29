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

namespace
{
    static const QString kUpnpBasicDeviceType("Basic");
    static const QString kHanwhaManufacturerName("Hanwha Techwin");
    static const QString kHanwhaAltManufacturerName("Samsung Techwin");
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

QList<QnResourcePtr> HanwhaResourceSearcher::checkHostAddr(
    const QUrl& url,
    const QAuthenticator& authOriginal,
    bool isSearchAction)
{
    return QList<QnResourcePtr>();
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

bool HanwhaResourceSearcher::isHanwhaCamera(const QString &vendorName, const QString& model) const
{
    const auto vendor = vendorName.toLower().trimmed();
    return vendor.startsWith(kHanwhaManufacturerName.toLower()) ||
        vendor.startsWith(kHanwhaAltManufacturerName.toLower());
}

bool HanwhaResourceSearcher::processPacket(
    const QHostAddress& /*discoveryAddr*/,
    const SocketAddress& deviceEndpoint,
    const nx_upnp::DeviceInfo& devInfo,
    const QByteArray& /*xmlDevInfo*/)
{
    if (!isHanwhaCamera(devInfo.manufacturer, devInfo.modelName))
        return false;

    QnMacAddress cameraMac(devInfo.udn.split(L'-').last());
    if (cameraMac.isNull())
        cameraMac = QnMacAddress(devInfo.serialNumber);

    QString model(devInfo.modelName);
    QnNetworkResourcePtr existingRes = resourcePool()->getResourceByMacAddress(cameraMac.toString());
    QAuthenticator cameraAuth;

    if (existingRes)
        cameraAuth = existingRes->getAuth();

	{
		QnMutexLocker lock(&m_mutex);

		if (m_alreadFoundMacAddresses.find(cameraMac.toString()) == m_alreadFoundMacAddresses.end())
		{
			m_alreadFoundMacAddresses.insert(cameraMac.toString());
			createResource( devInfo, cameraMac, cameraAuth, m_foundUpnpResources);
		}
	}

	return true;
}

void HanwhaResourceSearcher::createResource(
    const nx_upnp::DeviceInfo& devInfo,
    const QnMacAddress& mac,
    const QAuthenticator& auth,
    QnResourceList& result)
{

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), kHanwhaCameraName);
    if (rt.isNull())
        return;

    QnResourceData resourceData = qnStaticCommon->dataPool()->data(devInfo.manufacturer, devInfo.modelName);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return;

    HanwhaResourcePtr resource( new HanwhaResource() );

    resource->setTypeId(rt);
    resource->setVendor(devInfo.manufacturer);
    resource->setName(devInfo.modelName);
    resource->setModel(devInfo.modelName);
    resource->setUrl(devInfo.presentationUrl);
    resource->setMAC(mac);

    if (!auth.isNull()) 
        resource->setDefaultAuth(auth);

    result << resource;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
