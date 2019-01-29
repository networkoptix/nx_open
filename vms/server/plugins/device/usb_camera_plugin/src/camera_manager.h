#pragma once

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/helpers/ptr.h>

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
    CameraManager(const std::shared_ptr<Camera> camera);
    virtual ~CameraManager() = default;

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual int addRef() const override;
    virtual int releaseRef() const override;

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

    nxpt::CommonRefManager* refManager();

protected:
    std::shared_ptr<Camera> m_camera;

    nxpt::CommonRefManager m_refManager;
    nx::sdk::Ptr<Plugin> m_pluginRef;
    unsigned int m_capabilities;
    static int constexpr kEncoderCount = 2;
    std::unique_ptr<MediaEncoder> m_encoders[kEncoderCount];

    std::mutex m_mutex;
};

} // namespace usb_cam
} // namespace nx
