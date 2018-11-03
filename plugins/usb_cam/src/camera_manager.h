#pragma once

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include <vector>
#include <mutex>

#include "plugin.h"
#include "camera/camera.h"
#include "camera/codec_parameters.h"
#include "device/video/utils.h"
#include "device/abstract_compression_type_descriptor.h"

namespace nx {
namespace usb_cam {

class DiscoveryManager;
class MediaEncoder;

class CameraManager: public nxcip::BaseCameraManager2
{
public:
    CameraManager(
        DiscoveryManager * discoveryManager,
        nxpl::TimeProvider *const timeProvider,
        const nxcip::CameraInfo& info);
    virtual ~CameraManager() = default;

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

    virtual int createDtsArchiveReader(
        nxcip::DtsArchiveReader** dtsArchiveReader) const override;

    virtual int find(
        nxcip::ArchiveSearchOptions* searchOptions,
        nxcip::TimePeriods** timePeriods) const override;

    virtual int setMotionMask( nxcip::Picture* motionMask ) override;

    const nxcip::CameraInfo& info() const;
    nxpt::CommonRefManager* refManager();

    std::string getFfmpegUrl() const;

protected:
    DiscoveryManager * m_discoveryManager;
    nxpl::TimeProvider * const m_timeProvider;
    nxcip::CameraInfo m_info;
    nxpt::CommonRefManager m_refManager;
    nxpt::ScopedRef<Plugin> m_pluginRef;
    unsigned int m_capabilities;
    static int constexpr kEncoderCount = 2;
    std::unique_ptr<MediaEncoder> m_encoders[kEncoderCount];

    bool m_audioEnabled = false;
    std::shared_ptr<Camera> m_camera;

    std::mutex m_mutex;
};

} // namespace usb_cam
} // namespace nx 
