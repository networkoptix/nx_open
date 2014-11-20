#include "camera_manager.h"
#include "stream_reader.h"
#include "media_encoder.h"

#include "discovery_manager.h"

namespace ite
{
    DEFAULT_REF_COUNTER(MediaEncoder)

    MediaEncoder::MediaEncoder(CameraManager * const cameraManager, int encoderNumber)
    :   m_refManager( this ),
        m_cameraManager( cameraManager ),
        m_encoderNumber( encoderNumber )
    {
    }

    MediaEncoder::~MediaEncoder()
    {
    }

    void* MediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_CameraMediaEncoder2)
        {
            addRef();
            return static_cast<nxcip::CameraMediaEncoder2*>(this);
        }
        if (interfaceID == nxcip::IID_CameraMediaEncoder)
        {
            addRef();
            return static_cast<nxcip::CameraMediaEncoder*>(this);
        }
        if (interfaceID == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return NULL;
    }

    int MediaEncoder::getMediaUrl( char* urlBuf ) const
    {
        strcpy( urlBuf, m_cameraManager->info().url );
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const
    {
        infoList[0] = m_resolution;
        *infoListCount = 1;

        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::getMaxBitrate( int* maxBitrate ) const
    {
        *maxBitrate = 0;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setResolution( const nxcip::Resolution& resolution )
    {
        if (resolution.height != m_resolution.resolution.height || resolution.width != m_resolution.resolution.width)
            return nxcip::NX_UNSUPPORTED_RESOLUTION;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setFps( const float& /*fps*/, float* selectedFps )
    {
        *selectedFps = m_resolution.maxFps;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setBitrate( int /*bitrateKbps*/, int* /*selectedBitrateKbps*/ )
    {
        return nxcip::NX_NO_ERROR;
    }

    nxcip::StreamReader* MediaEncoder::getLiveStreamReader()
    {
        if (!m_streamReader.get())
            m_streamReader.reset( new StreamReader( &m_refManager, m_cameraManager, m_encoderNumber ) );

        m_streamReader->addRef();
        return m_streamReader.get();
    }

    int MediaEncoder::getAudioFormat( nxcip::AudioFormat* /*audioFormat*/ ) const
    {
        return nxcip::NX_UNSUPPORTED_CODEC;
    }
}
