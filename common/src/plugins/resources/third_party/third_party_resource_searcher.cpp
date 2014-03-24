/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include <algorithm>

#include <utils/common/log.h>

#include "third_party_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "core/resource_management/camera_driver_restriction_list.h"
#include "../../plugin_manager.h"


static const QLatin1String THIRD_PARTY_MANUFACTURER_NAME( "THIRD_PARTY" );

ThirdPartyResourceSearcher::ThirdPartyResourceSearcher( CameraDriverRestrictionList* cameraDriverRestrictionList )
:
    m_cameraDriverRestrictionList( cameraDriverRestrictionList )
{
    QList<nxcip::CameraDiscoveryManager*> pluginList = PluginManager::instance()->findNxPlugins<nxcip::CameraDiscoveryManager>( nxcip::IID_CameraDiscoveryManager );
    std::copy(
        pluginList.begin(),
        pluginList.end(),
        std::back_inserter(m_thirdPartyCamPlugins) );

    //reading and registering camera driver restrictions
    for( QList<nxcip_qt::CameraDiscoveryManager>::const_iterator
        it = m_thirdPartyCamPlugins.begin();
        it != m_thirdPartyCamPlugins.end();
        ++it )
    {
        const QList<QString>& modelList = it->getReservedModelList();
        foreach( QString modelMask, modelList )
            m_cameraDriverRestrictionList->allow( THIRD_PARTY_MANUFACTURER_NAME, it->getVendorName(), modelMask );
    }
}

ThirdPartyResourceSearcher::~ThirdPartyResourceSearcher()
{
}

QnResourcePtr ThirdPartyResourceSearcher::createResource( QnId resourceTypeId, const QnResourceParams& params )
{
    QnThirdPartyResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if( resourceType.isNull() )
    {
        NX_LOG( lit("ThirdPartyResourceSearcher. No resource type for ID = %1").arg(resourceTypeId.toString()), cl_logDEBUG1 );
        return result;
    }

    if( resourceType->getManufacture() != manufacture() )
    {
        //qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();
        return result;
    }

    nxcip::CameraInfo cameraInfo;
    //todo: #a.kolesnikov Check if only url parameter is enough. Create resource SHOULDN'T use a lot of parameters to create new resource instance
    strcpy( cameraInfo.url, params.url.toLatin1().constData() );
    

    //analyzing parameters, getting discoveryManager and filling in cameraInfo
    //QString resourceName;

    /*
    for( QnResourceParameters::const_iterator
        it = parameters.begin();
        it != parameters.end();
        ++it )
    {
        const QByteArray& valLatin1 = it.value().toLatin1();
        if( it.key() == "physicalId")
            strcpy( cameraInfo.uid, valLatin1.data() );
        else if( it.key() == "url")
            strcpy( cameraInfo.url, valLatin1.data() );
        else if( it.key() == "name")
            resourceName = it.value();
    }
    */

    nxcip_qt::CameraDiscoveryManager* discoveryManager = NULL;
    //choosing correct plugin
    for( QList<nxcip_qt::CameraDiscoveryManager>::iterator
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

    Q_ASSERT( discoveryManager->getRef() );

    const QByteArray& resourceNameLatin1 = params.vendor.toLatin1();
    strcpy( cameraInfo.modelName, resourceNameLatin1.data() + discoveryManager->getVendorName().size() + 1 );   //skipping vendor name and '-'

    nxcip::BaseCameraManager* camManager = discoveryManager->createCameraManager( cameraInfo );
    if( !camManager )
        return QnThirdPartyResourcePtr();

    result = QnThirdPartyResourcePtr( new QnThirdPartyResource( cameraInfo, camManager, *discoveryManager ) );
    result->setTypeId(resourceTypeId);
    result->setPhysicalId(QString::fromUtf8(cameraInfo.uid));

    unsigned int caps = 0;
    if (camManager->getCameraCapabilities(&caps) == 0) 
    {
        if( caps & nxcip::BaseCameraManager::shareIpCapability )
            result->setCameraCapability( Qn::ShareIpCapability, true );
    }

    NX_LOG( lit("Created third party resource (manufacturer %1, res type id %2)").
        arg(discoveryManager->getVendorName()).arg(resourceTypeId.toString()), cl_logDEBUG2 );

    //result->deserialize( parameters );
    return result;
}

QString ThirdPartyResourceSearcher::manufacture() const
{
    return THIRD_PARTY_MANUFACTURER_NAME;
}

QList<QnResourcePtr> ThirdPartyResourceSearcher::checkHostAddr( const QUrl& url, const QAuthenticator& auth, bool /*doMultichannelCheck*/ )
{
    QVector<nxcip::CameraInfo> cameraInfoTempArray;

    for( QList<nxcip_qt::CameraDiscoveryManager>::iterator
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
    const QHostAddress& discoveryAddress )
{
    QList<QnNetworkResourcePtr> localResults;

    for( QList<nxcip_qt::CameraDiscoveryManager>::iterator
        it = m_thirdPartyCamPlugins.begin();
        it != m_thirdPartyCamPlugins.end();
        ++it )
    {
        nxcip::CameraInfo cameraInfo;
        if( !it->fromMDNSData( responseData, discoveryAddress, &cameraInfo ) )
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
    const QString& /*host*/,
    const UpnpDeviceInfo& /*devInfo*/,
    const QByteArray& xmlDevInfo,
    QnResourceList& result )
{
    QList<QnNetworkResourcePtr> localResults;

    for( QList<nxcip_qt::CameraDiscoveryManager>::iterator
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
    QVector<nxcip::CameraInfo> cameraInfoTempArray;

    for( QList<nxcip_qt::CameraDiscoveryManager>::iterator
        it = m_thirdPartyCamPlugins.begin();
        it != m_thirdPartyCamPlugins.end();
        ++it )
    {
        int result = it->findCameras( &cameraInfoTempArray, QString() );
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
    foreach( nxcip::CameraInfo info, cameraInfoArray )
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
    QnId typeId = qnResTypePool->getResourceTypeId(manufacture(), THIRD_PARTY_MODEL_NAME);
    if( typeId.isNull() )
        return QnThirdPartyResourcePtr();

    nxcip::BaseCameraManager* camManager = discoveryManager->createCameraManager( cameraInfo );
    if( !camManager )
        return QnThirdPartyResourcePtr();

    discoveryManager->getRef()->addRef();   //this ref will be released by QnThirdPartyResource
    QnThirdPartyResourcePtr resource(new QnThirdPartyResource(cameraInfo, camManager, discoveryManager->getRef()));
    resource->setTypeId(typeId);
    resource->setName( QString::fromUtf8("%1-%2").arg(discoveryManager->getVendorName()).arg(QString::fromUtf8(cameraInfo.modelName)) );
    resource->setModel( QString::fromUtf8(cameraInfo.modelName) );
    resource->setMAC( QString::fromUtf8(cameraInfo.uid) );
    resource->setAuth( QString::fromUtf8(cameraInfo.defaultLogin), QString::fromUtf8(cameraInfo.defaultPassword) );
    resource->setUrl( QString::fromUtf8(cameraInfo.url) );
    resource->setPhysicalId( QString::fromUtf8(cameraInfo.uid) );
    resource->setVendor( discoveryManager->getVendorName() );
    
    unsigned int caps;
    if (camManager->getCameraCapabilities(&caps) == 0) 
    {
        if( caps & nxcip::BaseCameraManager::shareIpCapability )
            resource->setCameraCapability( Qn::ShareIpCapability, true );
    }

    QString groupName = QString(lit("%1-%2")).arg(discoveryManager->getVendorName()).arg(resource->getHostAddress());
    resource->setGroupName(groupName);
    resource->setGroupId(groupName.toLower().trimmed());
    return resource;
}
