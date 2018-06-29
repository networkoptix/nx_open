#pragma once

#include "stream_reader.h"

namespace nx { namespace ffmpeg { class StreamReader; } }
namespace nx { namespace ffmpeg { class Codec; } }
namespace nx { namespace ffmpeg { class Frame; } }

namespace nx {
namespace rpi_cam2 {

//!Transfers or transcodes packets from USB webcameras and streams them
class TranscodeStreamReader
:
    public StreamReader
{
public:
    TranscodeStreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        nxpl::TimeProvider *const timeProvider,
        const nxcip::CameraInfo& cameraInfo,
        const CodecContext& codecContext,
        const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~TranscodeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;

    virtual void setFps( int fps ) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;

private:
    std::unique_ptr<nx::ffmpeg::Codec> m_videoEncoder;

    bool m_initialized = false;
    bool m_modified = true;

    std::unique_ptr<ffmpeg::Frame> m_decodedFrame;
    std::unique_ptr<ffmpeg::Frame> m_scaledFrame;
    
private:
    int scale(AVFrame* frame, AVFrame * outFrame) const;
    int encode(const AVFrame * frame, AVPacket * outPacket) const;

    bool ensureInitialized();
    int initialize();
    void uninitialize();
    int openVideoEncoder();
    int initializeScaledFrame(const std::unique_ptr<ffmpeg::Codec>& encoder);
    void setEncoderOptions(const std::unique_ptr<ffmpeg::Codec>& encoder);

};

} // namespace rpi_cam2
} // namespace nx
