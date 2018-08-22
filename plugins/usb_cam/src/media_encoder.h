#pragma once

#include <memory>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>

#include "ffmpeg/codec_parameters.h"
// #include "ffmpeg/video_stream_reader.h"
// #include "ffmpeg/audio_stream_reader.h"
#include "camera.h"

namespace nx {
namespace usb_cam {

class CameraManager;
class StreamReader;

class MediaEncoder
:
    public nxcip::CameraMediaEncoder2
{
public:
     MediaEncoder(
        nxpt::CommonRefManager* const parentRefManager,
        int encoderIndex,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<Camera>& camera);

    virtual ~MediaEncoder();

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

    virtual int getMediaUrl( char* urlBuf ) const override;
    virtual int getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const override;
    virtual int getMaxBitrate( int* maxBitrate ) const override;
    virtual int setResolution( const nxcip::Resolution& resolution ) override;
    virtual int setFps( const float& fps, float* selectedFps ) override;
    virtual int setBitrate( int bitrateKbps, int* selectedBitrateKbps ) override;

    virtual nxcip::StreamReader* getLiveStreamReader() = 0;
    virtual int getAudioFormat( nxcip::AudioFormat* audioFormat ) const override;

    void updateCameraInfo(const nxcip::CameraInfo& info);

protected:
    nxpt::CommonRefManager m_refManager;
    int m_encoderIndex;
    ffmpeg::CodecParameters m_codecParams;
    std::shared_ptr<Camera> m_camera;

    std::shared_ptr<StreamReader> m_streamReader;
};

} // namespace nx 
} // namespace usb_cam 
