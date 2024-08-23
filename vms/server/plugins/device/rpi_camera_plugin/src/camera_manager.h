// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef RPI_CAMERA_MANAGER_H
#define RPI_CAMERA_MANAGER_H

#include <memory>
#include <string>

#include <camera/camera_plugin.h>

#include "ref_counter.h"

namespace rpi_cam
{
    class MediaEncoder;
    class RPiCamera;

    //!
    class CameraManager : public DefaultRefCounter<nxcip::BaseCameraManager>
    {
    public:
        CameraManager(const std::string& serial, const std::string& serverUrl);
        virtual ~CameraManager();

        // nxpl::PluginInterface

        virtual void * queryInterface( const nxpl::NX_GUID& interfaceID ) override;

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

    private:
        std::shared_ptr<RPiCamera> m_rpiCamera;
        std::shared_ptr<MediaEncoder> m_encoderHQ;
        std::shared_ptr<MediaEncoder> m_encoderLQ;

        nxcip::CameraInfo m_info;
        CameraParameters m_parameters;

        void makeInfo(const std::string& serial, const std::string& url);
    };
}

#endif
