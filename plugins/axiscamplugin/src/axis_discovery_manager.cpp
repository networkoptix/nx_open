/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "axis_discovery_manager.h"

#include <cstdlib>
#include <cstddef>
#include <cstring>

#include <QtNetwork/QAuthenticator>
#include <QtCore/QLatin1String>

#include "axis_cam_params.h"
#include "axis_camera_manager.h"
#include "axis_camera_plugin.h"
#include "sync_http_client.h"


AxisCameraDiscoveryManager::AxisCameraDiscoveryManager()
:
    m_refManager( AxisCameraPlugin::instance()->refManager() )
{
}

AxisCameraDiscoveryManager::~AxisCameraDiscoveryManager()
{
}

void* AxisCameraDiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int AxisCameraDiscoveryManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int AxisCameraDiscoveryManager::releaseRef()
{
    return m_refManager.releaseRef();
}

static const char* VENDOR_NAME = "AXIS";

void AxisCameraDiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int AxisCameraDiscoveryManager::findCameras( nxcip::CameraInfo* /*cameras*/, const char* /*localInterfaceIPAddr*/ )
{
    //supporting only MDNS-discovery
    return 0;
}

int AxisCameraDiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password )
{
    QString host;
    int port = DEFAULT_AXIS_API_PORT;

    const char* sepPos = strchr( address, ':' );
    if( sepPos )
    {
        host = QString::fromLatin1( address, sepPos-address );
        port = atoi( sepPos+1 );
    }
    else
    {
        //no port specified
        host = QString::fromLatin1( address );
    }

    QAuthenticator credentials;
    const char* loginToUse = login ? login : AXIS_DEFAULT_LOGIN;
    const char* passwordToUse = password ? password : AXIS_DEFAULT_PASSWORD;
    credentials.setUser( QString::fromUtf8(loginToUse) );
    credentials.setPassword( QString::fromUtf8(passwordToUse) );

    SyncHttpClient http(
        AxisCameraPlugin::instance()->networkAccessManager(),
        host,
        port,
        credentials );
    if( http.get( QLatin1String("/axis-cgi/param.cgi?action=list&group=root.Properties.Firmware.Version,root.Network.eth0.MACAddress,root.Brand.ProdShortName") ) != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;
    if( http.statusCode() != SyncHttpClient::HTTP_OK )
        return http.statusCode() == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;
    const QByteArray& responseBody = http.readWholeMessageBody();
    if( responseBody.isEmpty() )
        return 0;
    const QList<QByteArray>& lines = responseBody.split( '\n' );

    QByteArray modelName;
    QByteArray mac;
    QByteArray firmware;
    foreach( QByteArray line, lines )
    {
        //line has format param=value
        int paramValueSepPos = line.indexOf('=');
        if( paramValueSepPos == -1 )
            continue;   //unknown param format
        const QByteArray& paramName = line.mid(0, paramValueSepPos).trimmed();
        const QByteArray& paramValue = line.mid(paramValueSepPos+1).trimmed();

        if( paramName == "root.Brand.ProdShortName" )
        {
            modelName = paramValue;
            modelName.replace( QByteArray(" "), QByteArray() );
            modelName.replace( QByteArray("-"), QByteArray() );
        }
        else if( paramName == "root.Network.eth0.MACAddress" )
        {
            mac = paramValue;
        }
        else if( paramName == "root.Properties.Firmware.Version" )
        {
            firmware = paramValue;
        }
    }

    if( modelName.isEmpty() || mac.isEmpty() )
        return 0;

    memset( cameras, 0, sizeof(*cameras) );
    strncpy( cameras->uid, mac.constData(), sizeof(cameras->uid)-1 );
    strncpy( cameras->modelName, modelName.constData(), sizeof(cameras->modelName)-1 );
    strncpy( cameras->firmware, firmware.constData(), sizeof(cameras->firmware)-1 );
    const QByteArray& hostUtf8 = host.toUtf8();
    strncpy( cameras->url, hostUtf8.constData(), sizeof(cameras->url)-1 );
    strcpy( cameras->defaultLogin, loginToUse );
    strcpy( cameras->defaultPassword, passwordToUse );

    return 1;
}

int AxisCameraDiscoveryManager::fromMDNSData(
    const char* discoveredAddress,
    const unsigned char* mdnsResponsePacket,
    int mdnsResponsePacketSize,
    nxcip::CameraInfo* cameraInfo )
{
    return 0;


    const QByteArray& msdnResponse = QByteArray::fromRawData( (const char*)mdnsResponsePacket, mdnsResponsePacketSize );

    //parsing mdns response
    QByteArray smac;
    QByteArray name;

    int iqpos = msdnResponse.indexOf("AXIS");

    if (iqpos<0)
        return 0;

    int macpos = msdnResponse.indexOf("00", iqpos);
    if (macpos < 0)
        return 0;

    for (int i = iqpos; i < macpos; i++)
    {
        name += (char)msdnResponse[i];
    }

    name.replace(' ', QByteArray()); // remove spaces
    name.replace('-', QByteArray()); // remove spaces
    name.replace('\t', QByteArray()); // remove tabs

    if (macpos+12 > msdnResponse.size())
        return 0;

    //macpos++; // -

    while(msdnResponse.at(macpos)==' ')
        ++macpos;

    if (macpos+12 > msdnResponse.size())
        return 0;

    for (int i = 0; i < 12; i++)
    {
        if (i > 0 && i % 2 == 0)
            smac += '-';

        smac += (char)msdnResponse[macpos + i];
    }

    smac = smac.toUpper();

    memset( cameraInfo, 0, sizeof(*cameraInfo) );
    strncpy( cameraInfo->uid, smac.constData(), sizeof(cameraInfo->uid)-1 );
    strncpy( cameraInfo->modelName, name.constData(), sizeof(cameraInfo->modelName)-1 );
    strncpy( cameraInfo->defaultLogin, AXIS_DEFAULT_LOGIN, sizeof(cameraInfo->defaultLogin)-1 );
    strncpy( cameraInfo->defaultPassword, AXIS_DEFAULT_PASSWORD, sizeof(cameraInfo->defaultPassword)-1 );
    strncpy( cameraInfo->url, discoveredAddress, sizeof(cameraInfo->url)-1 );

    return 1;
}

int AxisCameraDiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
{
    //TODO/IMPL
    return 0;
}

nxcip::BaseCameraManager* AxisCameraDiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
{
    AxisCameraManager* obj = new AxisCameraManager( info );
    strcpy( obj->cameraInfo().defaultLogin, AXIS_DEFAULT_LOGIN );
    strcpy( obj->cameraInfo().defaultPassword, AXIS_DEFAULT_PASSWORD );
    return obj;
}

int AxisCameraDiscoveryManager::getReservedModelList( char** modelList, int* count )
{
#if 1
    if( !modelList )
    {
        *count = 1;
        return nxcip::NX_NO_ERROR;
    }

    if( *count >= 1 )
    {
        *count = 1;
        strcpy( modelList[0], "*1344*" );
    }
    else
    {
        *count = 1;
        return nxcip::NX_MORE_DATA;
    }
#else
    *count = 0;
#endif

    return nxcip::NX_NO_ERROR;
}
