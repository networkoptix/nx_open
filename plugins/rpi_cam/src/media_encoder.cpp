#include <vector>

#include "camera_manager.h"
#include "stream_reader.h"
#include "media_encoder.h"


namespace rpi_cam
{
    DEFAULT_REF_COUNTER(MediaEncoder)

    MediaEncoder::MediaEncoder(CameraManager * const cameraManager, int encoderNumber)
    :   m_refManager( this ),
        cameraManager_( cameraManager ),
        encoderNumber_( encoderNumber )
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

        return nullptr;
    }

    int MediaEncoder::getMediaUrl( char* urlBuf ) const
    {
        strcpy( urlBuf, cameraManager_->info().url );
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const
    {
        infoList[0] = resolution_;
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
        if (resolution.height != resolution_.resolution.height || resolution.width != resolution_.resolution.width)
            return nxcip::NX_UNSUPPORTED_RESOLUTION;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setFps( const float& /*fps*/, float* selectedFps )
    {
        *selectedFps = resolution_.maxFps;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setBitrate( int /*bitrateKbps*/, int* /*selectedBitrateKbps*/ )
    {
        return nxcip::NX_NO_ERROR;
    }

    nxcip::StreamReader* MediaEncoder::getLiveStreamReader()
    {
        if (!streamReader_.get())
            streamReader_.reset( new StreamReader( this ) );

        streamReader_->addRef();
        return streamReader_.get();
    }

    int MediaEncoder::getAudioFormat( nxcip::AudioFormat* /*audioFormat*/ ) const
    {
        return nxcip::NX_UNSUPPORTED_CODEC;
    }

    //

    nxcip::MediaDataPacket * MediaEncoder::nextFrame()
    {
        if (cameraManager_)
            return cameraManager_->nextFrame(encoderNumber_);
        return nullptr;
    }
}
