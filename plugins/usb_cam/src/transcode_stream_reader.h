#pragma once

#include "stream_reader.h"

#include <map>

namespace nx { namespace ffmpeg { class Codec; } }
namespace nx { namespace ffmpeg { class Frame; } }

namespace nx {
namespace usb_cam {

//!Transfers or transcodes packets from USB webcameras and streams them
class TranscodeStreamReader : public InternalStreamReader
{
public:
    TranscodeStreamReader(
        int encoderIndex,
        nxpl::TimeProvider *const timeProvider,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~TranscodeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** lpPacket ) override;
    virtual void interrupt() override;

    virtual void setFps(float fps) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;


private:
    enum StreamState 
    {
        kOff,
        kInitialized,
        kModified
    };
    StreamState m_cameraState;
    
    std::shared_ptr<nx::ffmpeg::Codec> m_encoder;
    std::unique_ptr<ffmpeg::Frame> m_scaledFrame;

    std::map<int64_t/*AVPacket.pts*/, int64_t/*ffmpeg::Frame.timeStamp()*/> m_timeStamps;
    int m_retries;

    std::shared_ptr<ffmpeg::BufferedFrameConsumer> m_consumer;
    bool m_added;
    bool m_interrupted;

private:
    int scale(AVFrame* frame, AVFrame * outFrame);
    void scaleNextFrame(const ffmpeg::Frame * frame, int * nxError);
    void encodeNextPacket(ffmpeg::Packet * outPacket, int * nxError);
    void flush();
    void addTimeStamp(int64_t ffmpegPts, int64_t nxTimeStamp);
    int64_t getNxTimeStamp(int64_t ffmpegPts);

    bool ensureInitialized();
    void ensureAdded();
    int initialize();
    void uninitialize();
    int openVideoEncoder();
    int initializeScaledFrame(const std::shared_ptr<ffmpeg::Codec>& encoder);
    void setEncoderOptions(const std::shared_ptr<ffmpeg::Codec>& encoder);
    void maybeDropFrames();
};

} // namespace usb_cam
} // namespace nx
