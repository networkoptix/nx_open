/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "third_party_resource_searcher.h"

#ifdef ENABLE_THIRD_PARTY

#include <algorithm>

#include <nx/utils/log/log.h>

#include "core/resource/camera_resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_management/camera_driver_restriction_list.h"
#include "core/resource_management/resource_data_pool.h"
#include "core/resource_management/resource_pool.h"
#include "common/common_module.h"
#include "plugins/plugin_manager.h"
#include <common/static_common_module.h>
#include <media_server/media_server_module.h>

static const QLatin1String THIRD_PARTY_MANUFACTURER_NAME( "THIRD_PARTY" );

ThirdPartyResourceSearcher::ThirdPartyResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule),
    QnMdnsResourceSearcher(commonModule),
    QnUpnpResourceSearcherAsync(commonModule)
{
    auto pluginManager = qnServerModule->pluginManager();
    NX_ASSERT(pluginManager, lit("There is no plugin manager"));
    QList<nxcip::CameraDiscoveryManager*> pluginList = pluginManager->findNxPlugins<nxcip::CameraDiscoveryManager>( nxcip::IID_CameraDiscoveryManager );
    std::copy(
        pluginList.begin(),
        pluginList.end(),
        std::back_inserter(m_thirdPartyCamPlugins) );

    //reading and registering camera driver restrictions
    for( auto
        it = m_thirdPartyCamPlugins.cbegin();
        it != m_thirdPartyCamPlugins.cend();
        ++it )
    {
        const QList<QString>& modelList = it->getReservedModelList();
        for(const  QString& modelMask: modelList )
            CameraDriverRestrictionList::instance()->allow( THIRD_PARTY_MANUFACTURER_NAME, it->getVendorName(), modelMask );
    }
}

ThirdPartyResourceSearcher::~ThirdPartyResourceSearcher()
{
}

QnResourcePtr ThirdPartyResourceSearcher::createResource( const QnUuid &resourceTypeId, const QnResourceParams& params )
{
    QnThirdPartyResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if( resourceType.isNull() )
    {
        NX_LOG( lit("ThirdPartyResourceSearcher. No resource type for ID = %1").arg(resourceTypeId.toString()), cl_logDEBUG1 );
        return result;
    }

    if( resourceType->getManufacture() != manufacture() )
        return result;

    nxcip::CameraInfo cameraInfo;
    //TODO #ak restore all resource cameraInfo fields here
    strcpy( cameraInfo.url, params.url.toLatin1().constData() );

    nxcip_qt::CameraDiscoveryManager* discoveryManager = NULL;
    //vendor is required to find plugin to use
    //choosing correct plugin
    for( auto
        it = m_thirdPartyCamPlugins.begin();
        it != m_thirdPartyCamPlugins.end();
        ++it )
    {
        if( params.vendor.startsWith(it->getVendorName()) )
        {
            discoveryManager = &*it;
            break;
        }
    }
    if( !discoveryManager )
        return QnThirdPartyResourcePtr();

    NX_ASSERT( discoveryManager->getRef() );
    //NOTE not calling discoveryManager->createCameraManager here because we do not know camera parameters (model, firmware, even uid),
        //so just instanciating QnThirdPartyResource
    result = QnThirdPartyResourcePtr( new QnThirdPartyResource( cameraInfo, nullptr, *discoveryManager ) );
    result->setTypeId(resourceTypeId);
    result->setPhysicalId(QString::fromUtf8(cameraInfo.uid).trimmed());

    NX_LOG( lit("Created third party resource (manufacturer %1, res type id %2)").
        arg(discoveryManager->getVendorName()).arg(resourceTypeId.toString()), cl_logDEBUG2 );

    return result;
}

QString ThirdPartyResourceSearcher::manufacture() const
{
    return THIRD_PARTY_MANUFACTURER_NAME;
}

QList<QnResourcePtr> ThirdPartyResourceSearcher::checkHostAddr( const nx::utils::Url& url, const QAuthenticator& auth, bool /*doMultichannelCheck*/ )
{
    QVector<nxcip::CameraInfo> cameraInfoTempArray;

    for( auto
        it = m_thirdPartyCamPlugins.begin();
        it != m_thirdPartyCamPlugins.end();
        ++it )
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
        int result = it->checkHostAddress(
            &cameraInfoTempArray,
            addressStr,
            &userName,
            &password );
        if( result == nxcip::NX_NOT_AUTHORIZED )
        {
            //trying one again with no login/password (so that plugin can use default ones)
            result = it->checkHostAddress(
                &cameraInfoTempArray,
                addressStr,
                NULL,
                NULL );
        }
        if( result <= 0 )
            continue;
        return createResListFromCameraInfoList( &*it, cameraInfoTempArray );
    }
    return QList<QnResourcePtr>();
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

    for( auto
        it = m_thirdPartyCamPlugins.begin();
        it != m_thirdPartyCamPlugins.end();
        ++it )
    {
        nxcip::CameraInfo cameraInfo;
        if( !it->fromMDNSData( responseData, foundHostAddress, &cameraInfo ) )
            continue;

        QnNetworkResourcePtr res = createResourceFromCameraInfo( &*it, cameraInfo );
        if( res )
            localResults.push_back( res );
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

    for( auto
        it = m_thirdPartyCamPlugins.begin();
        it != m_thirdPartyCamPlugins.end();
        ++it )
    {
        nxcip::CameraInfo cameraInfo;
        if( !it->fromUpnpData( xmlDevInfo, &cameraInfo ) )
            continue;

        QnNetworkResourcePtr res = createResourceFromCameraInfo( &*it, cameraInfo );
        if( res )
            result.push_back( res );
        return;
    }
}

QnResourceList ThirdPartyResourceSearcher::findResources()
{
    const QnResourceList& mdnsFoundResList = QnMdnsResourceSearcher::findResources();
    const QnResourceList& upnpFoundResList = QnUpnpResourceSearcherAsync::findResources();
    const QnResourceList& customFoundResList = doCustomSearch();
    return mdnsFoundResList + upnpFoundResList + customFoundResList;
}

QnResourceList ThirdPartyResourceSearcher::doCustomSearch()
{
    QString dafaultURL;
    QnMediaServerResourcePtr server = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    if( server )
        dafaultURL = server->getApiUrl().toString();

    QVector<nxcip::CameraInfo> cameraInfoTempArray;

    for( auto
        it = m_thirdPartyCamPlugins.begin();
        it != m_thirdPartyCamPlugins.end();
        ++it )
    {
        int result = it->findCameras( &cameraInfoTempArray, dafaultURL );
        if( result <= 0 )
            continue;

        return createResListFromCameraInfoList( &*it, cameraInfoTempArray );
    }
    return QnResourceList();
}

QnResourceList ThirdPartyResourceSearcher::createResListFromCameraInfoList(
    nxcip_qt::CameraDiscoveryManager* const discoveryManager,
    const QVector<nxcip::CameraInfo>& cameraInfoArray )
{
    QnResourceList resList;
    for(const  nxcip::CameraInfo& info: cameraInfoArray )
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
    const nxcip::CameraInfo& cameraInfo )
{
    const QString vendor = discoveryManager->getVendorName();

    if( strlen(cameraInfo.uid) == 0 )
    {
        NX_LOG( lit("THIRD_PARTY. Plugin %1 returned camera with empty uid. This is forbidden").
            arg(vendor), cl_logDEBUG1 );
        return QnThirdPartyResourcePtr();
    }
    if( strlen(cameraInfo.url) == 0 )
    {
        NX_LOG( lit("THIRD_PARTY. Plugin %1 returned camera with empty url. This is forbidden").
            arg(vendor), cl_logDEBUG1 );
        return QnThirdPartyResourcePtr();
    }

    QnUuid typeId = qnResTypePool->getResourceTypeId(manufacture(), THIRD_PARTY_MODEL_NAME);
    if( typeId.isNull() )
        return QnThirdPartyResourcePtr();

    nxcip::BaseCameraManager* camManager = discoveryManager->createCameraManager( cameraInfo );
    if( !camManager )
    {
        NX_LOG( lit("THIRD_PARTY. Plugin %1 could not create BaseCameraManager").arg(vendor), cl_logDEBUG1 );
        return QnThirdPartyResourcePtr();
    }

    discoveryManager->getRef()->addRef();   //this ref will be released by QnThirdPartyResource

    bool vendorIsRtsp = vendor == lit("GENERIC_RTSP");  //TODO #ak remove this!

    QnThirdPartyResourcePtr resource(new QnThirdPartyResource(cameraInfo, camManager, discoveryManager->getRef()));
    resource->setTypeId(typeId);
    if (vendorIsRtsp)
        resource->setName( QString::fromUtf8(cameraInfo.modelName) );
    else
        resource->setName( QString::fromUtf8("%1-%2").arg(vendor).arg(QString::fromUtf8(cameraInfo.modelName)) );
    resource->setModel( QString::fromUtf8(cameraInfo.modelName) );
    resource->setPhysicalId( QString::fromUtf8(cameraInfo.uid).trimmed() );
    resource->setMAC( nx::network::QnMacAddress(QString::fromUtf8(cameraInfo.uid).trimmed()) );
    resource->setDefaultAuth( QString::fromUtf8(cameraInfo.defaultLogin), QString::fromUtf8(cameraInfo.defaultPassword) );
    resource->setUrl( QString::fromUtf8(cameraInfo.url) );
    resource->setVendor( vendor );

    if( strlen(cameraInfo.auxiliaryData) > 0 )
        resource->setProperty( QnThirdPartyResource::AUX_DATA_PARAM_NAME, QString::fromLatin1(cameraInfo.auxiliaryData) );
    if( strlen(cameraInfo.firmware) > 0 )
        resource->setProperty( Qn::FIRMWARE_PARAM_NAME, QString::fromLatin1(cameraInfo.firmware) );

    if( !resourcePool()->getNetResourceByPhysicalId( resource->getPhysicalId() ) )
    {
        // new resource
        // TODO #ak reading MaxFPS here is a workaround of camera integration API defect:
        // it does not not allow plugin to return hard-coded max fps, it can only be read in during
        // init.
        const QnResourceData& resourceData = qnStaticCommon->dataPool()->data(resource);
        const float maxFps = resourceData.value<float>( Qn::MAX_FPS_PARAM_NAME, 0.0 );
        if( maxFps > 0.0 )
            resource->setProperty( Qn::MAX_FPS_PARAM_NAME, maxFps);
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
