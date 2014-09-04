#ifndef PACIDAL_CAMERA_MANAGER_H
#define PACIDAL_CAMERA_MANAGER_H

#include <vector>
#include <deque>
#include <memory>
#include <mutex>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"


namespace pacidal
{
    class PacidalIt930x;
    class PacidalStream;
    class MediaEncoder;
    class VideoPacket;

    struct LibAV;
    class DevReader;

    //!
    class CameraManager
    :
        public nxcip::BaseCameraManager2
    {
        DEF_REF_COUNTER

    public:
        CameraManager( const nxcip::CameraInfo& info );
        virtual ~CameraManager();

        //!Implementation of nxcip::BaseCameraManager::getEncoderCount
        virtual int getEncoderCount( int* encoderCount ) const override;
        //!Implementation of nxcip::BaseCameraManager::getEncoder
        virtual int getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr ) override;
        //!Implementation of nxcip::BaseCameraManager::getCameraInfo
        virtual int getCameraInfo( nxcip::CameraInfo* info ) const override;
        //!Implementation of nxcip::BaseCameraManager::getCameraCapabilities
        virtual int getCameraCapabilities( unsigned int* capabilitiesMask ) const override;
        //!Implementation of nxcip::BaseCameraManager::setCredentials
        virtual void setCredentials( const char* username, const char* password ) override;
        //!Implementation of nxcip::BaseCameraManager::setAudioEnabled
        virtual int setAudioEnabled( int audioEnabled ) override;
        //!Implementation of nxcip::BaseCameraManager::getPTZManager
        virtual nxcip::CameraPtzManager* getPtzManager() const override;
        //!Implementation of nxcip::BaseCameraManager::getCameraMotionDataProvider
        virtual nxcip::CameraMotionDataProvider* getCameraMotionDataProvider() const override;
        //!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
        virtual nxcip::CameraRelayIOManager* getCameraRelayIOManager() const override;
        //!Implementation of nxcip::BaseCameraManager::getLastErrorString
        virtual void getLastErrorString( char* errorString ) const override;

        //!Implementation of nxcip::BaseCameraManager2::createDtsArchiveReader
        virtual int createDtsArchiveReader( nxcip::DtsArchiveReader** dtsArchiveReader ) const override;
        //!Implementation of nxcip::BaseCameraManager2::find
        virtual int find( nxcip::ArchiveSearchOptions* searchOptions, nxcip::TimePeriods** timePeriods ) const override;
        //!Implementation of nxcip::BaseCameraManager2::setMotionMask
        virtual int setMotionMask( nxcip::Picture* motionMask ) override;

        const nxcip::CameraInfo& info() const { return m_info; }

        void resolution( unsigned encoderNum, nxcip::ResolutionInfo& out ) const;
        VideoPacket* nextPacket( unsigned encoderNumber );

    private:
        typedef std::shared_ptr<VideoPacket> VideoPacketPtr;
        typedef std::deque<VideoPacketPtr> VideoPacketQueue;

        std::mutex m_mutex;
        nxcip::CameraInfo m_info;

        std::unique_ptr<PacidalIt930x> m_device;
        std::unique_ptr<PacidalStream> m_devStream;
        std::unique_ptr<LibAV> m_libAV; // pimpl
        std::unique_ptr<DevReader> m_devReader;

        std::vector<std::shared_ptr<MediaEncoder>> m_encoders;
        std::vector<VideoPacketQueue> m_encQueues;
        bool devInited_;
        bool encInited_;

        void initDevice();
        void initEncoders();
        void readDeviceStream();

        std::unique_ptr<VideoPacket> nextDevPacket(unsigned& streamNum);
    };
}

#endif  //PACIDAL_CAMERA_MANAGER_H
