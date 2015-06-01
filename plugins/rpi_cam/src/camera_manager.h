#ifndef RPI_CAMERA_MANAGER_H
#define RPI_CAMERA_MANAGER_H

#include <memory>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "rpi_camera.h"

namespace rpi_cam
{
    class MediaEncoder;
    class RPiCamera;

    //!
    class CameraManager : public nxcip::BaseCameraManager
    {
        DEF_REF_COUNTER

    public:
        CameraManager();
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

        //

        std::shared_ptr<RPiCamera> rpiCamera() { return m_rpiCamera; }
        const nxcip::CameraInfo& info() const { return m_info; }

    private:
        std::shared_ptr<RPiCamera> m_rpiCamera;
        std::shared_ptr<MediaEncoder> m_encoderHQ;
        std::shared_ptr<MediaEncoder> m_encoderLQ;

        const char * m_errorStr;
        nxcip::CameraInfo m_info;
        CameraParameters m_parameters;
    };
}

#endif
