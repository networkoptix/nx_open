#pragma once

#include <memory>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "native_stream_reader.h"
#include "transcode_stream_reader.h"
#include "device/device_data.h"
#include "codec_context.h"

namespace nx{ namespace ffmpeg { class StreamReader; } }

namespace nx {
namespace rpi_cam2 {

class CameraManager;

class MediaEncoder
:
    public nxcip::CameraMediaEncoder2
{
public:
    MediaEncoder(
        int encoderIndex,
        CameraManager* const cameraManager,
        nxpl::TimeProvider *const timeProvider,
        const CodecContext& codecContext,
        const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader);

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

    void updateCameraInfo( const nxcip::CameraInfo& info );
    void setVideoCodecID(nxcip::CompressionType codecID);

protected:
    int m_encoderIndex;
    nxpt::CommonRefManager m_refManager;
    CameraManager* m_cameraManager;
    nxpl::TimeProvider *const m_timeProvider;
    CodecContext m_videoCodecContext;
    std::shared_ptr<nx::ffmpeg::StreamReader> m_ffmpegStreamReader;
    mutable int m_maxBitrate;

    std::shared_ptr<StreamReader> m_streamReader;
    
protected:
    QString decodeCameraInfoUrl() const;
};

} // namespace nx 
} // namespace rpi_cam2 
