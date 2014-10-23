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

        encoderHQ_ = std::make_shared<MediaEncoder>(this, 0, rpiCamera_->maxBitrate());
        for (unsigned i=0; VideoMode::num2mode(i) != VideoMode::VM_NONE; ++i)
        {
            VideoMode mode = VideoMode::num2mode(i);

            nxcip::ResolutionInfo res;
            res.maxFps = mode.framerate;
            res.resolution.width = mode.width;
            res.resolution.height = mode.height;
            encoderHQ_->addResolution(res);
        }

        if (ENCODERS_COUNT > 1)
        {
            VideoMode mode(VideoMode::modeLQ());

            encoderLQ_ = std::make_shared<MediaEncoder>(this, 1);
            nxcip::ResolutionInfo res;
            res.maxFps = mode.framerate;
            res.resolution.width = mode.width;
            res.resolution.height = mode.height;
            encoderLQ_->addResolution(res);
        }

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
                std::lock_guard<std::mutex> lock( mutex_ ); // LOCK

                if (rpiCamera_->nextFrameLQ(data))
                {
                    VideoPacket * packet = new VideoPacket(nxcip::CODEC_ID_MJPEG, &data[0], data.size());
                    packet->setLowQualityFlag();
                    return packet;
                }
            }
            else
            {
                std::lock_guard<std::mutex> lock( mutex_ ); // LOCK

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

    unsigned CameraManager::setBitrate(unsigned encoderNumber, unsigned bitrateKbps)
    {
        vcos_log_error("CameraManager::setBitrate");
#if 0
        if (encoderNumber > 0)
            return RaspberryPiCamera::maxBitrate();

        if (bitrateKbps > RaspberryPiCamera::maxBitrate())
            bitrateKbps = RaspberryPiCamera::maxBitrate();

        {
            std::lock_guard<std::mutex> lock( mutex_ ); // LOCK

            unsigned cameraNum = rpiCamera_->cameraNumber();
            VideoMode mode = rpiCamera_->mode();
            rpiCamera_.reset();
            rpiCamera_ = std::make_shared<RaspberryPiCamera>(cameraNum, bitrateKbps, mode);

            bitrateKbps = rpiCamera_->bitrate();
        }
#endif
        return bitrateKbps;
    }

    float CameraManager::setFps(unsigned encoderNumber, float fps)
    {
        vcos_log_error("CameraManager::setFps");
#if 0
        if (encoderNumber > 0)
            return RaspberryPiCamera::resolutionLQ().framerate;

        {
            std::lock_guard<std::mutex> lock( mutex_ ); // LOCK

            VideoMode mode = rpiCamera_->mode();

            if (fps < mode.framerate && fps >= 0.0f)
            {
                mode.framerate = fps;
                unsigned cameraNum = rpiCamera_->cameraNumber();
                unsigned bitrateKbps = rpiCamera_->bitrate();

                rpiCamera_.reset();
                rpiCamera_ = std::make_shared<RaspberryPiCamera>(cameraNum, bitrateKbps, mode);
            }

            fps = rpiCamera_->mode().framerate;
        }
#endif
        return fps;
    }

    bool CameraManager::setResolution(unsigned encoderNumber, unsigned width, unsigned height)
    {
        vcos_log_error("CameraManager::setResolution");
#if 0
        if (encoderNumber > 0)
            return false;

        VideoMode mode;
        bool found = false;
        for (unsigned i=0; VideoMode::num2mode(i) != VideoMode::VM_NONE; ++i)
        {
            mode = VideoMode::num2mode(i);
            if (mode.width == width && mode.height == height) {
                found = true;
                break;
            }
        }

        if (!found)
            return false;

        {
            std::lock_guard<std::mutex> lock( mutex_ ); // LOCK

            if (mode == rpiCamera_->mode())
                return true;

            unsigned cameraNum = rpiCamera_->cameraNumber();
            unsigned bitrateKbps = rpiCamera_->bitrate();

            rpiCamera_.reset();
            rpiCamera_ = std::make_shared<RaspberryPiCamera>(cameraNum, bitrateKbps, mode);
            return true;
        }

        return false;
#else
        return true;
#endif
    }
}
