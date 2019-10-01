/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "third_party_resource_searcher.h"

#ifdef ENABLE_THIRD_PARTY

#include <algorithm>

#include <nx/utils/log/log.h>
#include <nx/vms/server/discovery/discovery_common.h>

#include "core/resource/camera_resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_management/camera_driver_restriction_list.h"
#include "core/resource_management/resource_data_pool.h"
#include "core/resource_management/resource_pool.h"
#include "common/common_module.h"
#include "plugins/plugin_manager.h"
#include <media_server/media_server_module.h>

using namespace nx::vms::server::metrics;

static const QString kUpnpBasicDeviceType("Basic");

ThirdPartyResourceSearcher::ThirdPartyResourceSearcher(QnMediaServerModule* serverModule)
    :
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    QnMdnsResourceSearcher(serverModule),
    QnUpnpResourceSearcherAsync(serverModule, kUpnpBasicDeviceType),
    m_serverModule(serverModule)
{
    auto pluginManager = serverModule->pluginManager();
    NX_ASSERT(pluginManager, lit("There is no plugin manager"));

    const auto interfaceList = pluginManager->findNxPlugins<nxpl::PluginInterface>(
        nxpl::IID_PluginInterface);

    for (auto& pluginInterface: interfaceList)
    {
        nx::sdk::Ptr<nxpl::PluginInterface> interfacePtr = nx::sdk::toPtr(pluginInterface);
        auto discoveryManagerPtr =
            nx::sdk::queryInterfaceOfOldSdk<nxcip::CameraDiscoveryManager>(
                interfacePtr,
                nxcip::IID_CameraDiscoveryManager);

        if (!discoveryManagerPtr)
            continue;

        auto discoveryManagerWrapper =
            std::make_shared<nxcip_qt::CameraDiscoveryManager>(discoveryManagerPtr.releasePtr());

        nx::sdk::Ptr<nxpl::Plugin> pluginPtr =
            nx::sdk::queryInterfaceOfOldSdk<nxpl::Plugin>(interfacePtr, nxpl::IID_Plugin);

        QString pluginName;
        if (pluginPtr)
            pluginName = pluginPtr->name();

        if (pluginName.isEmpty())
            pluginName = discoveryManagerWrapper->getVendorName();

        if (pluginName.isEmpty())
            pluginName = "<Unknown Plugin>";

        if (m_discoveryManagerByPluginName.find(pluginName)
            != m_discoveryManagerByPluginName.cend())
        {
            NX_WARNING(this, "Plugins with equal names (%1) are found", pluginName);
            pluginName += "_" + QnUuid::createUuid().toString();
        }

        m_discoveryManagerByPluginName[pluginName] = discoveryManagerWrapper;
    }

    // Reading and registering device driver restrictions
    for(const auto& [pluginName, discoveryManager]: m_discoveryManagerByPluginName)
    {
        const QList<QString>& modelList = discoveryManager->getReservedModelList();
        for(const QString& modelMask: modelList)
        {
            m_serverModule->commonModule()->cameraDriverRestrictionList()
                ->allow(
                    nx::vms::server::discovery::kThirdPartyManufacturerName,
                    discoveryManager->getVendorName(),
                    modelMask);
        }
    }
}

ThirdPartyResourceSearcher::~ThirdPartyResourceSearcher()
{
}

QnResourcePtr ThirdPartyResourceSearcher::createResource(
    const QnUuid &resourceTypeId, const QnResourceParams& params )
{
    QnThirdPartyResourcePtr result;
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        NX_DEBUG(this, "No resource type for ID = %1", resourceTypeId);
        return result;
    }

    if (resourceType->getManufacturer() != manufacturer())
        return result;

    nxcip::CameraInfo cameraInfo;
    // TODO: #ak restore all resource cameraInfo fields here.
    strcpy(cameraInfo.url, params.url.toLatin1().constData());

    nxcip_qt::CameraDiscoveryManager* discoveryManager = NULL;
    // Vendor is required to find plugin to use.
    // Choosing correct plugin.
    for (auto& [pluginName, manager]: m_discoveryManagerByPluginName)
    {
        if (params.vendor.startsWith(manager->getVendorName()))
        {
            discoveryManager = manager.get();
            break;
        }
    }
    if (!discoveryManager)
        return QnThirdPartyResourcePtr();

    NX_ASSERT(discoveryManager->getRef());
    // NOTE: not calling discoveryManager->createCameraManager here because we do not know camera
    // parameters (model, firmware, even uid), so just instanciating QnThirdPartyResource.
    result = QnThirdPartyResourcePtr(
        new QnThirdPartyResource(m_serverModule, cameraInfo, nullptr, *discoveryManager));
    result->setTypeId(resourceTypeId);

    // If third party driver returns MAC based physical ID then re-format MAC address string
    // to ensure it has same string format as build-in drivers.
    auto uuidStr = QString::fromUtf8(cameraInfo.uid).trimmed();
    const auto uuidMac = nx::utils::MacAddress(uuidStr);
    if (!uuidMac.isNull())
        uuidStr = uuidMac.toString();
    result->setPhysicalId(uuidStr);

    NX_VERBOSE(this, "Created third party resource (manufacturer %1, res type id %2)",
        discoveryManager->getVendorName(), resourceTypeId);

    return result;
}

QString ThirdPartyResourceSearcher::manufacturer() const
{
    return nx::vms::server::discovery::kThirdPartyManufacturerName;
}

QList<QnResourcePtr> ThirdPartyResourceSearcher::checkHostAddr( const nx::utils::Url& url, const QAuthenticator& auth, bool /*doMultichannelCheck*/ )
{
    QVector<nxcip::CameraInfo2> cameraInfoTempArray;

    for(const auto& [pluginName, discoveryManager]: m_discoveryManagerByPluginName)
    {
        QString addressStr = url.scheme();
        if( url.scheme().isEmpty() )
        {
            //url is a host
            addressStr = url.toString(QUrl::RemoveScheme | QUrl::RemovePassword | QUrl::RemoveUserInfo | QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment);
            addressStr.remove( QLatin1Char('/') );
        }
        else
        {
            //url is an URL! or mswin path
            addressStr = QUrl::fromPercentEncoding(url.toString().toLatin1());
        }
        const QString& userName = auth.user();
        const QString& password = auth.password();
        int result = discoveryManager->checkHostAddress(
            &cameraInfoTempArray,
            addressStr,
            &userName,
            &password );
        if( result == nxcip::NX_NOT_AUTHORIZED )
        {
            //trying one again with no login/password (so that plugin can use default ones)
            result = discoveryManager->checkHostAddress(
                &cameraInfoTempArray,
                addressStr,
                NULL,
                NULL );
        }
        if( result <= 0 )
            continue;
        return createResListFromCameraInfoList(discoveryManager.get(), cameraInfoTempArray);
    }
    return QList<QnResourcePtr>();
}

std::vector<PluginMetrics> ThirdPartyResourceSearcher::metrics() const
{
    std::map</*plugin name*/ QString, PluginMetrics> pluginMetrics;
    const auto thirdPartyResources = resourcePool()->getAllCameras(
        resourcePool()->getOwnMediaServer()).filtered<QnThirdPartyResource>();

    for (const auto& thirdPartyResource: thirdPartyResources)
    {
        for (const auto& [pluginName, discoveryManager]: m_discoveryManagerByPluginName)
        {
            if (thirdPartyResource->getVendor().startsWith(discoveryManager->getVendorName()))
            {
                pluginMetrics[pluginName].name = pluginName;
                ++pluginMetrics[pluginName].numberOfBoundResources;
                const Qn::ResourceStatus status = thirdPartyResource->getStatus();
                if (status == Qn::Online || status == Qn::Recording)
                    ++pluginMetrics[pluginName].numberOfAliveBoundResources;

                break;
            }
        }
    }

    std::vector<PluginMetrics> result;
    for (auto& [pluginName, pluginMetrics]: pluginMetrics)
        result.push_back(std::move(pluginMetrics));

    return result;
}

static ThirdPartyResourceSearcher* globalInstance = NULL;

void ThirdPartyResourceSearcher::initStaticInstance( ThirdPartyResourceSearcher* _instance )
{
    globalInstance = _instance;
}

ThirdPartyResourceSearcher* ThirdPartyResourceSearcher::instance()
{
    return globalInstance;
}

QList<QnNetworkResourcePtr> ThirdPartyResourceSearcher::processPacket(
    QnResourceList& /*result*/,
    const QByteArray& responseData,
    const QHostAddress& /*discoveryAddress*/,
    const QHostAddress& foundHostAddress )
{
    QList<QnNetworkResourcePtr> localResults;

    for (const auto& [pluginName, discoveryManager]: m_discoveryManagerByPluginName)
    {
        nxcip::CameraInfo cameraInfo;
        if (!discoveryManager->fromMDNSData(responseData, foundHostAddress, &cameraInfo))
            continue;

        QnNetworkResourcePtr res = createResourceFromCameraInfo(
            discoveryManager.get(),
            cameraInfo);

        if (res)
            localResults.push_back(res);

        break;
    }

    return localResults;
}

void ThirdPartyResourceSearcher::processPacket(
    const QHostAddress& /*discoveryAddr*/,
    const nx::network::SocketAddress& /*host*/,
    const nx::network::upnp::DeviceInfo& /*devInfo*/,
    const QByteArray& xmlDevInfo,
    QnResourceList& result )
{
    QList<QnNetworkResourcePtr> localResults;

    for(const auto& [pluginName, discoveryManager]: m_discoveryManagerByPluginName)
    {
        nxcip::CameraInfo cameraInfo;
        if (!discoveryManager->fromUpnpData(xmlDevInfo, &cameraInfo))
            continue;

        QnNetworkResourcePtr res = createResourceFromCameraInfo(
            discoveryManager.get(),
            cameraInfo);

        if (res)
            result.push_back(res);

        return;
    }
}

QnResourceList ThirdPartyResourceSearcher::findResources()
{
    const QnResourceList& mdnsFoundResList = QnMdnsResourceSearcher::findResources();
    const QnResourceList& upnpFoundResList = QnUpnpResourceSearcherAsync::findResources();
    const QnResourceList& customFoundResList = doCustomSearch();

    NX_DEBUG(this, lm("Found %1 mdns, %2 upnp and %3 customSearch resources")
        .args(mdnsFoundResList.size(), upnpFoundResList.size(), customFoundResList.size()));
    return mdnsFoundResList + upnpFoundResList + customFoundResList;
}

QnResourceList ThirdPartyResourceSearcher::doCustomSearch()
{
    QString dafaultURL;
    QnMediaServerResourcePtr server = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    if( server )
        dafaultURL = server->getApiUrl().toString();

    QVector<nxcip::CameraInfo2> cameraInfoTempArray;

    for(const auto& [pluginName, discoveryManager]: m_discoveryManagerByPluginName)
    {
        int result = discoveryManager->findCameras(&cameraInfoTempArray, dafaultURL);
        NX_VERBOSE(this, "Find cameras for custom plugin [%1] (URL: %2) returned [%3]",
            discoveryManager->getVendorName(), dafaultURL, result);

        if (result <= 0)
            continue;

        return createResListFromCameraInfoList(discoveryManager.get(), cameraInfoTempArray);
    }

    NX_VERBOSE(this, "Custom search did not find any cameras");
    return QnResourceList();
}

QnResourceList ThirdPartyResourceSearcher::createResListFromCameraInfoList(
    nxcip_qt::CameraDiscoveryManager* const discoveryManager,
    const QVector<nxcip::CameraInfo2>& cameraInfoArray )
{
    QnResourceList resList;
    for(const auto& info: cameraInfoArray )
    {
        QnThirdPartyResourcePtr res = createResourceFromCameraInfo( discoveryManager, info );
        if( !res )
            continue;
        resList.push_back( res );
    }
    return resList;
}

static const QLatin1String THIRD_PARTY_MODEL_NAME( "THIRD_PARTY_COMMON" );

QnThirdPartyResourcePtr ThirdPartyResourceSearcher::createResourceFromCameraInfo(
    nxcip_qt::CameraDiscoveryManager* const discoveryManager,
    const nxcip::CameraInfo2& cameraInfo )
{
    const QString vendor = discoveryManager->getVendorName();

    if( strlen(cameraInfo.uid) == 0 )
    {
        NX_DEBUG(this, lit("Plugin %1 returned camera with empty uid. This is forbidden").
            arg(vendor));
        return QnThirdPartyResourcePtr();
    }
    if( strlen(cameraInfo.url) == 0 )
    {
        NX_DEBUG(this, lit("Plugin %1 returned camera with empty url. This is forbidden").
            arg(vendor));
        return QnThirdPartyResourcePtr();
    }

    QnUuid typeId = qnResTypePool->getResourceTypeId(manufacturer(), THIRD_PARTY_MODEL_NAME);
    if( typeId.isNull() )
        return QnThirdPartyResourcePtr();

    nxcip::BaseCameraManager* camManager = discoveryManager->createCameraManager( cameraInfo );
    if( !camManager )
    {
        NX_DEBUG(this, lit("Plugin %1 could not create BaseCameraManager").arg(vendor));
        return QnThirdPartyResourcePtr();
    }

    discoveryManager->getRef()->addRef();   //this ref will be released by QnThirdPartyResource

    QnThirdPartyResourcePtr resource(new QnThirdPartyResource(
        m_serverModule, cameraInfo, camManager, discoveryManager->getRef()));
    resource->setTypeId(typeId);
    auto model = QString::fromUtf8(cameraInfo.modelName);
    if (!QUrl(model).scheme().isEmpty())
        resource->setName(model);
    else
        resource->setName(QString::fromUtf8("%1-%2").arg(vendor).arg(model));
    resource->setModel(model);

    const auto uuid = QString::fromUtf8(cameraInfo.uid).trimmed();
    const auto uuidMac = nx::utils::MacAddress(uuid);
    resource->setPhysicalId(uuidMac.isNull() ? uuid : uuidMac.toString());
    resource->setMAC(uuidMac);
    resource->setDefaultAuth( QString::fromUtf8(cameraInfo.defaultLogin), QString::fromUtf8(cameraInfo.defaultPassword) );
    resource->setUrl( QString::fromUtf8(cameraInfo.url) );
    resource->setVendor( vendor );

    resource->setGroupId(QString::fromUtf8(cameraInfo.groupId));
    resource->setDefaultGroupName(QString::fromUtf8(cameraInfo.groupName));

    if( strlen(cameraInfo.auxiliaryData) > 0 )
        resource->setProperty( QnThirdPartyResource::AUX_DATA_PARAM_NAME, QString::fromLatin1(cameraInfo.auxiliaryData) );
    if( strlen(cameraInfo.firmware) > 0 )
        resource->setProperty(ResourcePropertyKey::kFirmware, QString::fromLatin1(cameraInfo.firmware));

    if( !resourcePool()->getNetResourceByPhysicalId( resource->getPhysicalId() ) )
    {
        // new resource
        // TODO #ak reading MaxFPS here is a workaround of camera integration API defect:
        // it does not allow plugin to return hard-coded max fps, it can only be read in during
        // init.
        const QnResourceData& resourceData = resource->resourceData();
        const auto maxFps = resourceData.value<float>(ResourceDataKey::kMaxFps, 0.0);
        if( maxFps > 0.0 )
            resource->setMaxFps(maxFps);
    }

    unsigned int caps;
    if (camManager->getCameraCapabilities(&caps) == 0)
    {
        if( caps & nxcip::BaseCameraManager::shareIpCapability )
            resource->setCameraCapability( Qn::ShareIpCapability, true );
    }

    /*
    if (!vendorIsRtsp) { //Bug #3276: Remove group element for generic rtsp/http links
        QString groupName = QString(lit("%1-%2")).arg(vendor).arg(resource->getHostAddress());
        resource->setGroupName(groupName);
        resource->setGroupId(groupName.toLower().trimmed());
    }
    */
    return resource;
}

#endif // ENABLE_THIRD_PARTY
