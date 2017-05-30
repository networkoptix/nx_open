#include <cstring>

#include "generic_multicast_media_encoder.h"
#include "generic_multicast_camera_manager.h"

GenericMulticastMediaEncoder::GenericMulticastMediaEncoder( GenericMulticastCameraManager* const cameraManager )
:
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager )
{
}

GenericMulticastMediaEncoder::~GenericMulticastMediaEncoder()
{
}

void* GenericMulticastMediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraMediaEncoder, sizeof(nxcip::IID_CameraMediaEncoder) ) == 0 )
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

unsigned int GenericMulticastMediaEncoder::addRef()
{
    return m_refManager.addRef();
}

unsigned int GenericMulticastMediaEncoder::releaseRef()
{
    return m_refManager.releaseRef();
}

int GenericMulticastMediaEncoder::getMediaUrl( char* urlBuf ) const
{
    strcpy( urlBuf, m_cameraManager->info().url );
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastMediaEncoder::getResolutionList( nxcip::ResolutionInfo* /*infoList*/, int* infoListCount ) const
{
    *infoListCount = 0;
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastMediaEncoder::getMaxBitrate( int* maxBitrate ) const
{
    *maxBitrate = 0;
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastMediaEncoder::setResolution( const nxcip::Resolution& /*resolution*/ )
{
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastMediaEncoder::setFps( const float& /*fps*/, float* /*selectedFps*/ )
{
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastMediaEncoder::setBitrate( int /*bitrateKbps*/, int* /*selectedBitrateKbps*/ )
{
    return nxcip::NX_NO_ERROR;
}
