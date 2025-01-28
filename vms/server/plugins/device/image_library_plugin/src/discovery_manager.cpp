// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "discovery_manager.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#endif
#include <cstdio>

#include "camera_manager.h"
#include "dir_iterator.h"
#include "image_library_plugin.h"
#include "utils.h"

#include <nx/kit/debug.h>

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
    if (memcmp(&interfaceID, &nxcip::IID_CameraDiscoveryManager2, sizeof(nxcip::IID_CameraDiscoveryManager2)) == 0)
    {
        addRef();
        return this;
    }
    if (memcmp(&interfaceID, &nxcip::IID_CameraDiscoveryManager3, sizeof(nxcip::IID_CameraDiscoveryManager3)) == 0)
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

int DiscoveryManager::addRef() const
{
    return m_refManager.addRef();
}

int DiscoveryManager::releaseRef() const
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

int DiscoveryManager::findCameras2(nxcip::CameraInfo2* /*cameras*/, const char* /*serverURL*/)
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int DiscoveryManager::checkHostAddress(
    nxcip::CameraInfo* cameraInfo,
    const char* address,
    const char* /*login*/,
    const char* /*password*/ )
{
    using nx::kit::utils::toString;

    const std::string path = fileUrlToPath(address);

    if (path.empty())
    {
        NX_PRINT << "The file path in the URL is empty.";
        return 0;
    }
    if (path.size() > FILENAME_MAX)
    {
        NX_PRINT << "The file path in the URL is longer than " << FILENAME_MAX << " characters.";
        return 0;
    }

    struct stat fStat;
    memset(&fStat, 0, sizeof(fStat));
    if (const int e = stat(path.c_str(), &fStat); e != 0)
    {
        NX_PRINT << "stat(" << toString(path) << ") failed with code " << e << ".";
        return 0;
    }

    if (!(fStat.st_mode & S_IFDIR))
    {
        NX_PRINT << "The URL path is not a directory.";
        return 0;
    }

    // Checking whether the directory contains jpg images.
    DirIterator dirIterator(path);
    //m_dirIterator.setRecursive(true);
    dirIterator.setEntryTypeFilter(FsEntryType::etRegularFile);
    dirIterator.setWildCardMask("*.jp*g"); //< jpg or jpeg
    if (!dirIterator.next())
    {
        NX_PRINT << "The directory " << toString(path) << " contains no .jpg or .jpeg files.";
        return 0;
    }

    strcpy(cameraInfo->url, address); //< Use the full URL in this field.
    strcpy(cameraInfo->uid, path.c_str());
    strcpy(cameraInfo->modelName, path.c_str());

    logCameraInfo(*cameraInfo, std::string("DiscoveryManager::") + __func__ + "()");

    return 1;
}

//!Implementation of nxcip::CameraDiscoveryManager2::checkHostAddress2
int DiscoveryManager::checkHostAddress2(nxcip::CameraInfo2* cameras, const char* address, const char* login, const char* password)
{
    return checkHostAddress(cameras, address, login, password);
}

int DiscoveryManager::fromMDNSData(
    const char* /*discoveredAddress*/,
    const unsigned char* /*mdnsResponsePacket*/,
    int /*mdnsResponsePacketSize*/,
    nxcip::CameraInfo* /*cameraInfo*/ )
{
    return 0;
}

int DiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
{
    return 0;
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

int DiscoveryManager::getCapabilities() const
{
    return Capability::findLocalResources;
}
