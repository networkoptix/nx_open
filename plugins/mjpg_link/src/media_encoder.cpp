/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "media_encoder.h"

#include "camera_manager.h"
#include "stream_reader.h"


static const float MIN_FPS = 1.0 / 86400.0; //once per day
static const float MAX_FPS = 30;

MediaEncoder::MediaEncoder(
    CameraManager* const cameraManager,
    int encoderNumber )
:
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager ),
    m_encoderNumber( encoderNumber ),
    m_currentFps( MAX_FPS )
{
}

MediaEncoder::~MediaEncoder()
{
}

void* MediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraMediaEncoder2, sizeof(nxcip::IID_CameraMediaEncoder2) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder2*>(this);
    }
    if( memcmp( &interfaceID, &nxcip::IID_CameraMediaEncoder, sizeof(nxcip::IID_CameraMediaEncoder) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder*>(this);
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

int MediaEncoder::getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const
{
#if 0
    if( !m_resolutionKnown.isValid() )
    {
        //TODO: #ak reading one picture to get its resolution
        std::unique_ptr<StreamReader> streamReader( new StreamReader(
            &m_refManager,
            m_cameraManager->info(),
            m_frameDurationUsec,
            m_encoderNumber ) );

        nxcip::MediaDataPacket* mediaPacket = nullptr;
        if( streamReader->getNextData( &mediaPacket ) == nxcip::NX_NO_ERROR && mediaPacket )
        {
            QBuffer picBuf;
            picBuf.setData( mediaPacket->data(), mediaPacket->dataSize() );
            //this requires QtGui, which is not supposed to be used on mediaserver
            QImageReader imageReader( &picBuf, "jpg" );
        }
    }

    infoList[0].resolution.width = m_resolutionKnown.width();
    infoList[0].resolution.height = m_resolutionKnown.height();
    infoList[0].maxFps = 30;
#else
    infoList[0].resolution.width = 800;
    infoList[0].resolution.height = 450;
    infoList[0].maxFps = MAX_FPS;
    *infoListCount = 1;
#endif

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

int MediaEncoder::setFps( const float& fps, float* selectedFps )
{
    m_currentFps = std::min<float>( std::max<float>( fps, MIN_FPS ), MAX_FPS );
    *selectedFps = m_currentFps;
    if( m_streamReader.get() )
        m_streamReader->setFps( m_currentFps );
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setBitrate( int bitrateKbps, int* selectedBitrateKbps )
{
    *selectedBitrateKbps = bitrateKbps;
    return nxcip::NX_NO_ERROR;
}

nxcip::StreamReader* MediaEncoder::getLiveStreamReader()
{
    if( !m_streamReader.get() )
        m_streamReader.reset( new StreamReader(
            &m_refManager,
            m_cameraManager->info(),
            m_currentFps,
            m_encoderNumber ) );

    m_streamReader->addRef();
    return m_streamReader.get();
}

int MediaEncoder::getAudioFormat( nxcip::AudioFormat* audioFormat ) const
{
    return nxcip::NX_UNSUPPORTED_CODEC;
}

void MediaEncoder::updateCameraInfo( const nxcip::CameraInfo& info )
{
    if( m_streamReader.get() )
        m_streamReader->updateCameraInfo( info );
}
