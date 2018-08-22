#pragma once

#include "stream_reader.h"
#include "ffmpeg/buffered_stream_consumer.h"

#include <map>
#include <deque>

#include "ffmpeg/timestamp_mapper.h"

namespace nx { namespace ffmpeg { class Codec; } }
namespace nx { namespace ffmpeg { class Frame; } }

namespace nx {
namespace usb_cam {

//!Transfers or transcodes packets from USB webcameras and streams them
class TranscodeStreamReader : public StreamReaderPrivate
{
public:
    TranscodeStreamReader(
        int encoderIndex,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<Camera>& camera);
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
    int m_retries;
    int m_initCode;

    std::shared_ptr<ffmpeg::BufferedVideoFrameConsumer> m_consumer;
    
    std::unique_ptr<nx::ffmpeg::Codec> m_encoder;
    std::unique_ptr<ffmpeg::Frame> m_scaledFrame;

    ffmpeg::TimeStampMapper m_timeStamps;
    
private:
    std::shared_ptr<ffmpeg::Packet> transcodeNextPacket(int * nxError);
    void scaleNextFrame(const ffmpeg::Frame * frame, int * nxError);
    void encode(ffmpeg::Packet * outPacket, int * nxError);
    void flush();
    void addTimeStamp(int64_t ffmpegPts, int64_t nxTimeStamp);
    int64_t getNxTimeStamp(int64_t ffmpegPts);

    std::shared_ptr<ffmpeg::Packet> nextPacket(nxcip::DataPacketType * outMediaType, int * outNxError);

    bool ensureInitialized();
    void ensureConsumerAdded() override;
    int initialize();
    void uninitialize();
    int openVideoEncoder();
    int initializeScaledFrame(const ffmpeg::Codec* encoder);
    void setEncoderOptions(ffmpeg::Codec* encoder);
    void maybeDropFrames();
    int scale(AVFrame * frame, AVFrame* outFrame);
};

} // namespace usb_cam
} // namespace nx
