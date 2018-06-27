#pragma once

#include "stream_reader.h"

namespace nx {
namespace webcam_plugin {

namespace ffmpeg { class StreamReader; }
namespace ffmpeg { class Codec; }

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
        const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~TranscodeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;

    virtual void setFps( int fps ) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;

private:
    std::unique_ptr<ffmpeg::Codec> m_videoEncoder;

    bool m_initialized = false;
    bool m_modified = true;

private:
    int scale(AVFrame* frame, AVFrame ** outFrame) const;
    int encode(AVFrame * frame, AVPacket * outPacket) const;

    bool ensureInitialized();
    int initialize();
    void uninitialize();
    int openVideoEncoder();
    void setEncoderOptions(const std::unique_ptr<ffmpeg::Codec>&  encoder);

};

} // namespace webcam_plugin
} // namespace nx
