/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#include "generic_rtsp_media_encoder.h"

#include <cstring>

#include "generic_rtsp_camera_manager.h"


GenericRTSPMediaEncoder::GenericRTSPMediaEncoder(
    GenericRTSPCameraManager* const cameraManager, int encoderIndex)
:
    m_refManager(cameraManager->refManager()),
    m_cameraManager(cameraManager),
    m_encoderIndex(encoderIndex)
{
    memset(m_mediaUrl, 0, sizeof(m_mediaUrl));
}

GenericRTSPMediaEncoder::~GenericRTSPMediaEncoder()
{
}

void* GenericRTSPMediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
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

unsigned int GenericRTSPMediaEncoder::addRef()
{
    return m_refManager.addRef();
}

unsigned int GenericRTSPMediaEncoder::releaseRef()
{
    return m_refManager.releaseRef();
}

int GenericRTSPMediaEncoder::getMediaUrl(char* urlBuf) const
{
    urlBuf[0] = 0;
    if (m_mediaUrl[0])
        strcpy(urlBuf, m_mediaUrl);
    else if (m_encoderIndex == 0)
        strcpy(urlBuf, m_cameraManager->info().url);

    return nxcip::NX_NO_ERROR;
}

int GenericRTSPMediaEncoder::setMediaUrl(const char url[nxcip::MAX_TEXT_LEN])
{
    strncpy(m_mediaUrl, url, nxcip::MAX_TEXT_LEN);
    m_mediaUrl[nxcip::MAX_TEXT_LEN - 1] = '\0';
    return nxcip::NX_NO_ERROR;
}

int GenericRTSPMediaEncoder::getResolutionList( nxcip::ResolutionInfo* /*infoList*/, int* infoListCount ) const
{
    *infoListCount = 0;
    return nxcip::NX_NO_ERROR;
}

int GenericRTSPMediaEncoder::getMaxBitrate( int* maxBitrate ) const
{
    *maxBitrate = 0;
    return nxcip::NX_NO_ERROR;
}

int GenericRTSPMediaEncoder::setResolution( const nxcip::Resolution& /*resolution*/ )
{
    return nxcip::NX_NO_ERROR;
}

int GenericRTSPMediaEncoder::setFps( const float& /*fps*/, float* /*selectedFps*/ )
{
    return nxcip::NX_NO_ERROR;
}

int GenericRTSPMediaEncoder::setBitrate( int /*bitrateKbps*/, int* /*selectedBitrateKbps*/ )
{
    return nxcip::NX_NO_ERROR;
}
