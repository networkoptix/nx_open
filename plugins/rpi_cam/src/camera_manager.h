#ifndef RPI_CAMERA_MANAGER_H
#define RPI_CAMERA_MANAGER_H

#include <memory>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"

namespace rpi_cam
{
    class MediaEncoder;
    class RaspberryPiCamera;

    //!
    class CameraManager : public nxcip::BaseCameraManager2
    {
        DEF_REF_COUNTER

    public:
        CameraManager(unsigned id);
        virtual ~CameraManager();

        // nxcip::BaseCameraManager

        virtual int getEncoderCount( int* encoderCount ) const override;
        virtual int getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr ) override;
        virtual int getCameraInfo( nxcip::CameraInfo* info ) const override;
        virtual int getCameraCapabilities( unsigned int* capabilitiesMask ) const override;
        virtual void setCredentials( const char* username, const char* password ) override;
        virtual int setAudioEnabled( int audioEnabled ) override;
        virtual nxcip::CameraPtzManager* getPtzManager() const override;
        virtual nxcip::CameraMotionDataProvider* getCameraMotionDataProvider() const override;
        virtual nxcip::CameraRelayIOManager* getCameraRelayIOManager() const override;
        virtual void getLastErrorString( char* errorString ) const override;

        // nxcip::BaseCameraManager2

        virtual int createDtsArchiveReader( nxcip::DtsArchiveReader** dtsArchiveReader ) const override;
        virtual int find( nxcip::ArchiveSearchOptions* searchOptions, nxcip::TimePeriods** timePeriods ) const override;
        virtual int setMotionMask( nxcip::Picture* motionMask ) override;

        //

        const nxcip::CameraInfo& info() const { return info_; }
        bool isOK() const { return isOK_; }
        bool setResolution(unsigned width, unsigned height);

        nxcip::MediaDataPacket * nextFrame(unsigned encoderNumber);

    private:
        std::shared_ptr<RaspberryPiCamera> rpiCamera_;
        std::shared_ptr<MediaEncoder> encoderHQ_;
        std::shared_ptr<MediaEncoder> encoderLQ_;
        bool isOK_;

        const char * errorStr_;
        nxcip::CameraInfo info_;
    };
}

#endif
