#pragma once

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include <vector>

#include "ffmpeg/codec_parameters.h"
#include "plugin.h"

namespace nx { namespace ffmpeg { class StreamReader; } }

namespace nx {
namespace rpi_cam2 {

class MediaEncoder;

class CameraManager
:
    public nxcip::BaseCameraManager2
{
public:
    CameraManager(const nxcip::CameraInfo& info, nxpl::TimeProvider *const timeProvider);
    virtual ~CameraManager();

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

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

    virtual int createDtsArchiveReader( nxcip::DtsArchiveReader** dtsArchiveReader ) const override;
    virtual int find( nxcip::ArchiveSearchOptions* searchOptions, nxcip::TimePeriods** timePeriods ) const override;
    virtual int setMotionMask( nxcip::Picture* motionMask ) override;

    const nxcip::CameraInfo& info() const;
    nxpt::CommonRefManager* refManager();

    std::string decodeCameraInfoUrl() const;
    
protected:
    nxcip::CameraInfo m_info;
    nxpl::TimeProvider *const m_timeProvider;
    nxpt::CommonRefManager m_refManager;
    nxpt::ScopedRef<Plugin> m_pluginRef;
    unsigned int m_capabilities;
    std::vector<std::unique_ptr<MediaEncoder>> m_encoders;

    std::shared_ptr<ffmpeg::StreamReader> m_ffmpegStreamReader;
};

} // namespace rpi_cam2
} // namespace nx 
