/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "media_encoder.h"

#include "camera_manager.h"


MediaEncoder::MediaEncoder( CameraManager* const cameraManager )
:
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager )
{
}

MediaEncoder::~MediaEncoder()
{
}

void* MediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
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

unsigned int MediaEncoder::addRef()
{
    return m_refManager.addRef();
}

unsigned int MediaEncoder::releaseRef()
{
    return m_refManager.releaseRef();
}

int MediaEncoder::getMediaUrl( char* urlBuf ) const
{
    strcpy( urlBuf, m_cameraManager->info().url );
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getResolutionList( nxcip::ResolutionInfo* /*infoList*/, int* infoListCount ) const
{
    *infoListCount = 0;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getMaxBitrate( int* maxBitrate ) const
{
    *maxBitrate = 0;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setResolution( const nxcip::Resolution& /*resolution*/ )
{
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setFps( const float& /*fps*/, float* /*selectedFps*/ )
{
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setBitrate( int /*bitrateKbps*/, int* /*selectedBitrateKbps*/ )
{
    return nxcip::NX_NO_ERROR;
}
