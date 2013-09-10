/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "discovery_manager.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#endif
#include <cstdio>

#include "camera_manager.h"
#include "dir_iterator.h"
#include "plugin.h"


DiscoveryManager::DiscoveryManager()
:
    m_refManager( ImageLibraryPlugin::instance()->refManager() )
{
}

void* DiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
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

unsigned int DiscoveryManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int DiscoveryManager::releaseRef()
{
    return m_refManager.releaseRef();
}

static const char* VENDOR_NAME = "IMAGE_LIBRARY";

void DiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int DiscoveryManager::findCameras( nxcip::CameraInfo* /*cameras*/, const char* /*localInterfaceIPAddr*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int DiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* /*login*/, const char* /*password*/ )
{
    const size_t addressLen = strlen( address );
    if( addressLen == 0 || addressLen > FILENAME_MAX )
        return 0;

    struct stat fStat;
    memset( &fStat, 0, sizeof(fStat) );

    if( address[addressLen-1] == '/' || address[addressLen-1] == '\\' )
    {
        //removing trailing separator
        char tmpNameBuf[FILENAME_MAX+1];
        strcpy( tmpNameBuf, address );
        for( char* pos = tmpNameBuf+addressLen-1;
            pos >= tmpNameBuf && (*pos == '/' || *pos == '\\');
            --pos )
        {
            *pos = '\0';
        }
        if( stat( tmpNameBuf, &fStat ) != 0 )
            return 0;
    }
    else
    {
        if( stat( address, &fStat ) != 0 )
            return 0;
    }

    if( !(fStat.st_mode & S_IFDIR) )
        return 0;

    //address is a path to local directory

    //checking, whether address dir contains jpg images
    DirIterator dirIterator( address );

    //m_dirIterator.setRecursive( true );
    dirIterator.setEntryTypeFilter( FsEntryType::etRegularFile );
    dirIterator.setWildCardMask( "*.jp*g" );    //jpg or jpeg

    //reading directory
    bool isImageLibrary = false;
    while( dirIterator.next() )
    {
        isImageLibrary = true;
        break;
    }
    if( !isImageLibrary )
        return 0;

    strcpy( cameras[0].url, address );
    strcpy( cameras[0].uid, address );
    strcpy( cameras[0].modelName, address );

    return 1;
}

int DiscoveryManager::fromMDNSData(
    const char* /*discoveredAddress*/,
    const unsigned char* /*mdnsResponsePacket*/,
    int /*mdnsResponsePacketSize*/,
    nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NO_ERROR;
}

int DiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NO_ERROR;
}

nxcip::BaseCameraManager* DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
{
    const nxcip::CameraInfo& infoCopy( info );
    if( strlen(infoCopy.auxiliaryData) == 0 )
    {
        //TODO/IMPL checking, if audio is present at info.url
    }
    return new CameraManager( infoCopy );
}

int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
{
    *count = 0;
    return nxcip::NX_NO_ERROR;
}
