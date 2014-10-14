#include <vector>

#include "camera_manager.h"
#include "stream_reader.h"
#include "media_encoder.h"


namespace rpi_cam
{
    DEFAULT_REF_COUNTER(MediaEncoder)

    MediaEncoder::MediaEncoder(CameraManager * const cameraManager, unsigned encoderNumber, unsigned maxBitrate)
    :   m_refManager( this ),
        cameraManager_(cameraManager),
        encoderNumber_(encoderNumber),
        maxBitrate_(maxBitrate)
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
        *infoListCount = 0;
        for (unsigned i=0; i < resolutions_.size(); ++i)
        {
            infoList[i] = resolutions_[i];
            ++(*infoListCount);
        }

        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::getMaxBitrate( int* maxBitrate ) const
    {
        *maxBitrate = maxBitrate_;
        return nxcip::NX_NO_ERROR;
    }

    // TODO
    int MediaEncoder::setResolution( const nxcip::Resolution& res )
    {
        unsigned num = 0;
        for (; num < resolutions_.size(); ++num)
        {
            if (res.width == resolutions_[num].resolution.width &&
                res.height == resolutions_[num].resolution.height)
                break;
        }

        if (num == resolutions_.size())
            return nxcip::NX_UNSUPPORTED_RESOLUTION;

        if (cameraManager_->setResolution(res.width, res.height))
            return nxcip::NX_NO_ERROR;
        return nxcip::NX_OTHER_ERROR;
    }

    // TODO
    int MediaEncoder::setFps( const float& /*fps*/, float* selectedFps )
    {
        *selectedFps = resolutions_[0].maxFps;
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
