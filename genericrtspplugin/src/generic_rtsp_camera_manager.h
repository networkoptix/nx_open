/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#ifndef GENERIC_RTSP_CAMERA_MANAGER_H
#define GENERIC_RTSP_CAMERA_MANAGER_H

#include <memory>

#include <plugins/camera_plugin.h>

#include "common_ref_manager.h"
#include "generic_rtsp_plugin.h"


class GenericRTSPMediaEncoder;

class GenericRTSPCameraManager
:
    public nxcip::BaseCameraManager,
    public CommonRefManager
{
public:
    GenericRTSPCameraManager( const nxcip::CameraInfo& info );
    virtual ~GenericRTSPCameraManager();

    //!Implementation of nxpl::NXPluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;

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
    virtual nxcip::CameraPTZManager* getPTZManager() const override;
    //!Implementation of nxcip::BaseCameraManager::getCameraMotionDataProvider
    virtual nxcip::CameraMotionDataProvider* getCameraMotionDataProvider() const override;
    //!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
    virtual nxcip::CameraRelayIOManager* getCameraRelayIOManager() const override;
    //!Implementation of nxcip::BaseCameraManager::getLastErrorString
    virtual void getLastErrorString( char* errorString ) const override;

    const nxcip::CameraInfo& info() const;

private:
    /*!
        Holding reference to \a AxisCameraPlugin, but not \a AxisCameraDiscoveryManager, 
        since \a AxisCameraDiscoveryManager instance is not required for \a AxisCameraManager object
    */
    nxpl::ScopedRef<GenericRTSPPlugin> m_pluginRef;
    nxcip::CameraInfo m_info;
    unsigned int m_capabilities;
    std::auto_ptr<GenericRTSPMediaEncoder> m_encoder;
};

#endif  //GENERIC_RTSP_CAMERA_MANAGER_H
