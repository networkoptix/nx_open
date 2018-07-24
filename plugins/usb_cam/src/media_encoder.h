#pragma once

#include <memory>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>

#include "ffmpeg/codec_parameters.h"

namespace nx{ namespace ffmpeg { class StreamReader; } }

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
        int encoderIndex,
        const ffmpeg::CodecParameters& codecParams,
        CameraManager* const cameraManager,
        nxpl::TimeProvider *const timeProvider,
        const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader);

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

    int lastFfmpegError() const;

protected:
    int m_encoderIndex;
    ffmpeg::CodecParameters m_codecParams;
    nxpt::CommonRefManager m_refManager;
    CameraManager* m_cameraManager;
    nxpl::TimeProvider *const m_timeProvider;

    std::shared_ptr<nx::ffmpeg::StreamReader> m_ffmpegStreamReader;
    std::shared_ptr<StreamReader> m_streamReader;
    
protected:
    std::string decodeCameraInfoUrl() const;
};

} // namespace nx 
} // namespace usb_cam 
