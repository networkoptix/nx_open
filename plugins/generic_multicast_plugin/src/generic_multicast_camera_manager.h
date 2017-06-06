#pragma once

#include <memory>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_tools.h>

#include "generic_multicast_plugin.h"


class GenericMulticastMediaEncoder;

class GenericMulticastCameraManager
:
    public nxcip::BaseCameraManager3
{
public:
    GenericMulticastCameraManager( const nxcip::CameraInfo& info );
    virtual ~GenericMulticastCameraManager();

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

    bool isAudioEnabled() const;

    const nxcip::CameraInfo& info() const;
    nxpt::CommonRefManager* refManager();

private:
    nxpt::CommonRefManager m_refManager;
    nxpt::ScopedRef<GenericMulticastPlugin> m_pluginRef;
    nxcip::CameraInfo m_info;
    unsigned int m_capabilities;
    std::unique_ptr<GenericMulticastMediaEncoder> m_encoder;
    bool m_audioEnabled;
};
