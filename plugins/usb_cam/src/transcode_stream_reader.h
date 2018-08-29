#pragma once

#include "stream_reader.h"

#include <map>
#include <deque>

#include "camera/buffered_stream_consumer.h"
#include "camera/timestamp_mapper.h"

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
        const CodecParameters& codecParams,
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
        csOff,
        csInitialized,
        csModified
    };
    StreamState m_cameraState;
    int m_retries;
    int m_initCode;

    uint64_t m_lastVideoTimeStamp;
    uint64_t m_timePerFrame;
    int m_framesToDrop;
    int m_frameDropCount;
    std::shared_ptr<BufferedVideoFrameConsumer> m_consumer;
    
    std::unique_ptr<nx::ffmpeg::Codec> m_encoder;
    std::unique_ptr<ffmpeg::Frame> m_scaledFrame;

    TimeStampMapper m_timeStamps;

    bool m_needFramesDropped;
    
    std::shared_ptr<ffmpeg::Packet> m_encodedVideoPacket;
    bool m_checkNextAudio;

private:
    std::shared_ptr<ffmpeg::Packet> transcodeVideo(const ffmpeg::Frame * frame, int * outNxError);
    void scaleNextFrame(const ffmpeg::Frame * frame, int * outNxError);
    void encode(ffmpeg::Packet * outPacket, int * outNxError);
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
    void dropIfBuffersTooFull();
    int scale(AVFrame * frame, AVFrame* outFrame);
    void calculateTimePerFrame();
    int framesToDrop();
};

} // namespace usb_cam
} // namespace nx
