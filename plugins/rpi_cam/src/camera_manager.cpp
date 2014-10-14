#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <memory>

#include "camera_manager.h"
#include "media_encoder.h"
#include "video_packet.h"

#include "discovery_manager.h"
#include "raspi_cam.h"

namespace rpi_cam
{
    static unsigned ENCODERS_COUNT = 1;

    DEFAULT_REF_COUNTER(CameraManager)

    CameraManager::CameraManager(unsigned cameraNum)
    :   m_refManager( DiscoveryManager::refManager() ),
        isOK_(false),
        errorStr_(nullptr)
    {
        rpiCamera_ = std::make_shared<RaspberryPiCamera>(cameraNum);
        if (rpiCamera_->isOK() && rpiCamera_->startCapture())
            isOK_ = true;

        VideoMode m(VideoMode::VM_1080p30);
        encoderHQ_ = std::make_shared<MediaEncoder>(this, 0, rpiCamera_->maxBitrate());
        for (unsigned i=0; rpiCamera_->resolutionHQ(i, m); ++i)
        {
            nxcip::ResolutionInfo res;
            res.maxFps = m.framerate;
            res.resolution.width = m.width;
            res.resolution.height = m.height;
            encoderHQ_->addResolution(res);
        }
#if 0
        encoderLQ_ = std::make_shared<MediaEncoder>(this, 1);
        for (unsigned i=0; rpiCamera_->resolutionLQ(i, m); ++i)
        {
            nxcip::ResolutionInfo res;
            res.maxFps = m.framerate;
            res.resolution.width = m.width;
            res.resolution.height = m.height;
            encoderLQ_->addResolution(res);
        }
#endif
        // TODO
        std::string strId = "RaspberryPi";

        memset( &info_, 0, sizeof(nxcip::CameraInfo) );
        strncpy( info_.url, strId.c_str(), std::min(strId.size(), sizeof(nxcip::CameraInfo::url)-1) );
        strncpy( info_.uid, strId.c_str(), std::min(strId.size(), sizeof(nxcip::CameraInfo::uid)-1) );
        strncpy( info_.modelName, strId.c_str(), std::min(strId.size(), sizeof(nxcip::CameraInfo::uid)-1) );
    }

    CameraManager::~CameraManager()
    {
        if (encoderHQ_)
            encoderHQ_->releaseRef();

        if (encoderLQ_)
            encoderLQ_->releaseRef();
    }

    void* CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_BaseCameraManager2)
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager2*>(this);
        }
        if (interfaceID == nxcip::IID_BaseCameraManager)
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager*>(this);
        }

        return nullptr;
    }

    int CameraManager::getEncoderCount( int* encoderCount ) const
    {
        if (! isOK_) {
            *encoderCount = 0;
            return nxcip::NX_TRY_AGAIN;
        }

        *encoderCount = ENCODERS_COUNT;
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
    {
        if (static_cast<unsigned>(encoderIndex) >= ENCODERS_COUNT)
            return nxcip::NX_INVALID_ENCODER_NUMBER;

        if (encoderIndex)
        {
            if (encoderLQ_)
                encoderLQ_->addRef();
            *encoderPtr = encoderLQ_.get();
        }
        else
        {
            encoderHQ_->addRef();
            *encoderPtr = encoderHQ_.get();
        }
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getCameraInfo( nxcip::CameraInfo * info ) const
    {
        memcpy( info, &info_, sizeof(info_) );
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
    {
        *capabilitiesMask = nxcip::BaseCameraManager::nativeMediaStreamCapability;
        return nxcip::NX_NO_ERROR;
    }

    void CameraManager::setCredentials( const char* /*username*/, const char* /*password*/ )
    {
    }

    int CameraManager::setAudioEnabled( int /*audioEnabled*/ )
    {
        return nxcip::NX_NO_ERROR;
    }

    nxcip::CameraPtzManager* CameraManager::getPtzManager() const
    {
        return nullptr;
    }

    nxcip::CameraMotionDataProvider* CameraManager::getCameraMotionDataProvider() const
    {
        return nullptr;
    }

    nxcip::CameraRelayIOManager* CameraManager::getCameraRelayIOManager() const
    {
        return nullptr;
    }

    void CameraManager::getLastErrorString( char * errorString ) const
    {
        if (errorString)
        {
            errorString[0] = '\0';
            if (errorStr_)
                strncpy( errorString, errorStr_, nxcip::MAX_TEXT_LEN );
        }
    }

    int CameraManager::createDtsArchiveReader( nxcip::DtsArchiveReader** /*dtsArchiveReader*/ ) const
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int CameraManager::find( nxcip::ArchiveSearchOptions* /*searchOptions*/, nxcip::TimePeriods** /*timePeriods*/ ) const
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int CameraManager::setMotionMask( nxcip::Picture* /*motionMask*/ )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    //

    bool CameraManager::setResolution(unsigned width, unsigned height)
    {
        return rpiCamera_->setResolution(width, height);
    }

    nxcip::MediaDataPacket * CameraManager::nextFrame(unsigned encoderNumber)
    {
        if (encoderNumber >= ENCODERS_COUNT)
            return nullptr;

        static const unsigned MAX_READ_ATTEMPTS = 20;
        static const unsigned SLEEP_DURATION_MS = 50;

        static std::chrono::milliseconds dura( SLEEP_DURATION_MS );

        std::vector<uint8_t> data;
        int64_t pts = 0;
        bool isKey = false;

        for (unsigned i = 0; i < MAX_READ_ATTEMPTS; ++i)
        {
            if (encoderNumber)
            {
#if 0
                if (rpiCamera_->nextFrameLQ(data))
                {
                    VideoPacket * packet = new VideoPacket(nxcip::CODEC_ID_MJPEG, &data[0], data.size());
                    packet->setLowQualityFlag();
                    return packet;
                }
#endif
            }
            else
            {
                if (rpiCamera_->nextFrame(data, pts, isKey))
                {
                    VideoPacket * packet = new VideoPacket(nxcip::CODEC_ID_H264, &data[0], data.size());
                    packet->setTime(pts);
                    if (isKey)
                        packet->setKeyFlag();
                    return packet;
                }
            }

            std::this_thread::sleep_for(dura);
        }

        return nullptr;
    }
}
