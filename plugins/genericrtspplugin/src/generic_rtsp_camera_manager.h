/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#ifndef GENERIC_RTSP_CAMERA_MANAGER_H
#define GENERIC_RTSP_CAMERA_MANAGER_H

#include <memory>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include <plugins/plugin_tools.h>
#include "generic_rtsp_plugin.h"


class GenericRTSPMediaEncoder;

class GenericRTSPCameraManager
:
    public nxcip::BaseCameraManager3
{
public:
    GenericRTSPCameraManager( const nxcip::CameraInfo& info );
    virtual ~GenericRTSPCameraManager();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

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
    //!Implementation of nxcip::BaseCameraManager::c
    virtual void getLastErrorString( char* errorString ) const override;


    //!Implementation of nxcip::BaseCameraManager2::createDtsArchiveReader
    virtual int createDtsArchiveReader( nxcip::DtsArchiveReader** dtsArchiveReader ) const override;
    //!Implementation of nxcip::BaseCameraManager2::find
    virtual int find( nxcip::ArchiveSearchOptions* searchOptions, nxcip::TimePeriods** timePeriods ) const override;
    //!Implementation of nxcip::BaseCameraManager2::setMotionMask
    virtual int setMotionMask( nxcip::Picture* motionMask ) override;


    //!Implementation of nxcip::BaseCameraManager3::getParametersDescriptionXML
    virtual const char* getParametersDescriptionXML() const override;
    //!Implementation of nxcip::BaseCameraManager3::getParamValue
    virtual int getParamValue( const char* paramName, char* valueBuf, int* valueBufSize ) const override;
    //!Implementation of nxcip::BaseCameraManager3::setParamValue
    virtual int setParamValue( const char* paramName, const char* value ) override;


    const nxcip::CameraInfo& info() const;
    nxpt::CommonRefManager* refManager();

private:
    nxpt::CommonRefManager m_refManager;
    /*!
        Holding reference to \a AxisCameraPlugin, but not \a AxisCameraDiscoveryManager, 
        since \a AxisCameraDiscoveryManager instance is not required for \a AxisCameraManager object
    */
    nxpt::ScopedRef<GenericRTSPPlugin> m_pluginRef;
    nxcip::CameraInfo m_info;
    unsigned int m_capabilities;
    std::auto_ptr<GenericRTSPMediaEncoder> m_encoder;
};

#endif  //GENERIC_RTSP_CAMERA_MANAGER_H
