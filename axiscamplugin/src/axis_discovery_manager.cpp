/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "axis_discovery_manager.h"

#include <cstddef>
#include <cstring>

#include <QLatin1String>

#include "axis_camera_manager.h"


AxisCameraDiscoveryManager::AxisCameraDiscoveryManager()
:
    m_refCount( 1 )
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

unsigned int AxisCameraDiscoveryManager::addRef()
{
    return m_refCount.fetchAndAddOrdered(1) + 1;
}

unsigned int AxisCameraDiscoveryManager::releaseRef()
{
    unsigned int newRefCounter = m_refCount.fetchAndAddOrdered(-1) - 1;
    if( newRefCounter == 0 )
        delete this;
    return newRefCounter;
}

static const char* VENDOR_NAME = "AXIS";

void AxisCameraDiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int AxisCameraDiscoveryManager::findCameras( nxcip::CameraInfo* /*cameras*/, const char* /*localInterfaceIPAddr*/ )
{
    //supporting only UPNP-discovery
    return 0;
}

int AxisCameraDiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* url, const char* login, const char* password )
{
    //TODO/IMPL
    return 0;
}

static const char* AXIS_DEFAULT_LOGIN = "root";
static const char* AXIS_DEFAULT_PASSWORD = "root";

int AxisCameraDiscoveryManager::fromMDNSData(
    const char* discoveredAddress,
    const unsigned char* mdnsResponsePacket,
    int mdnsResponsePacketSize,
    nxcip::CameraInfo* cameraInfo )
{
    const QByteArray& msdnResponse = QByteArray::fromRawData( (const char*)mdnsResponsePacket, mdnsResponsePacketSize );

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

    strcpy( cameraInfo->uid, smac );
    strcpy( cameraInfo->modelName, name );
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
    return new AxisCameraManager( info );
}
