/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "axis_discovery_manager.h"

#include <cstddef>
#include <cstring>

#include <QAuthenticator>
#include <QLatin1String>

#include <utils/network/simple_http_client.h>

#include "axis_camera_manager.h"
#include "axis_cam_params.h"


AxisCameraDiscoveryManager::AxisCameraDiscoveryManager()
//:
//    m_refCount( 1 )
{
}

void* AxisCameraDiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        addRef();
        return this;
    }
    return NULL;
}

//unsigned int AxisCameraDiscoveryManager::addRef()
//{
//    return m_refCount.fetchAndAddOrdered(1) + 1;
//}
//
//unsigned int AxisCameraDiscoveryManager::releaseRef()
//{
//    unsigned int newRefCounter = m_refCount.fetchAndAddOrdered(-1) - 1;
//    if( newRefCounter == 0 )
//        delete this;
//    return newRefCounter;
//}

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

    CLSimpleHTTPClient http( host, port, DEFAULT_SOCKET_READ_WRITE_TIMEOUT_MS, credentials );
    CLHttpStatus status = http.doGET( QByteArray("axis-cgi/param.cgi?action=list&group=root.Properties.Firmware.Version,root.Network.eth0.MACAddress,root.Network.Bonjour.FriendlyName") );
    if( status != CL_HTTP_SUCCESS )
        return status == CL_HTTP_AUTH_REQUIRED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;
    QByteArray responseBody;
    http.readAll( responseBody );
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

        if( paramName == "root.Network.Bonjour.FriendlyName" )  //has format: <product name> - <serialnumber>
            modelName = paramValue.mid(0, paramValue.indexOf('-')).trimmed();
        else if( paramName == "root.Network.eth0.MACAddress" )
            mac = paramValue;
        else if( paramName == "root.Properties.Firmware.Version" )
            firmware = paramValue;
    }

    if( modelName.isEmpty() || mac.isEmpty() )
        return 0;

    memset( cameras, 0, sizeof(*cameras) );
    strncpy( cameras->uid, mac.data(), std::min<int>(sizeof(cameras->uid)-1, mac.size()) );
    strncpy( cameras->modelName, modelName.data(), std::min<int>(sizeof(cameras->modelName)-1, modelName.size()) );
    strncpy( cameras->firmware, firmware.data(), std::min<int>(sizeof(cameras->firmware)-1, firmware.size()) );
    const QByteArray& hostUtf8 = host.toUtf8();
    strncpy( cameras->url, hostUtf8.data(), std::min<int>(sizeof(cameras->url)-1, hostUtf8.size()) );
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
    strncpy( cameraInfo->uid, smac.data(), std::min<int>(sizeof(cameraInfo->uid)-1, smac.size()) );
    strncpy( cameraInfo->modelName, name.data(), std::min<int>(sizeof(cameraInfo->modelName)-1, name.size()) );
    strcpy( cameraInfo->defaultLogin, AXIS_DEFAULT_LOGIN );
    strcpy( cameraInfo->defaultPassword, AXIS_DEFAULT_PASSWORD );
    strcpy( cameraInfo->url, discoveredAddress );

    return 1;
}

int AxisCameraDiscoveryManager::fromUpnpData( const char* upnpXMLData, int upnpXMLDataSize, nxcip::CameraInfo* cameraInfo )
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
