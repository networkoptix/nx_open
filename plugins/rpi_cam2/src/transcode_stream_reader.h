#pragma once

#include "stream_reader.h"

#include <map>

#include <nx/utils/thread/sync_queue.h>

struct SwsContext;

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
        int encoderIndex,
        nxpt::CommonRefManager* const parentRefManager,
        nxpl::TimeProvider *const timeProvider,
        const nxcip::CameraInfo& cameraInfo,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~TranscodeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;

    virtual void setFps( int fps ) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;

private:
    enum StreamState 
    {
        kOff,
        kInitialized,
        kModified
    };
    StreamState m_state;
    
    std::shared_ptr<nx::ffmpeg::Codec> m_encoder;
    std::unique_ptr<nx::ffmpeg::Codec> m_decoder;

    std::unique_ptr<ffmpeg::Frame> m_decodedFrame;
    std::unique_ptr<ffmpeg::Frame> m_scaledFrame;
    
    struct SwsContext * m_scaleContext = nullptr;

    bool m_started = false;
    bool m_terminated = false;
    std::mutex m_mutex;
    std::thread m_runThread;
    nx::utils::SyncQueue<std::shared_ptr<ffmpeg::Frame>> m_scaledFrames;

    std::map<int64_t/*AVPacket.pts*/, int64_t/*ffmpeg::Packet.timeStamp()*/> m_timeStamps;

private:
    void start();
    void stop();
    void run();
    int scale(AVFrame* frame, AVFrame * outFrame);
    int encode(const ffmpeg::Frame * frame, ffmpeg::Packet * outPacket);
    int decode (AVFrame * outFrame, const AVPacket * packet, int * gotFrame);
    void decodeNextFrame(int * nxError);
    void scaleNextFrame(int * nxError);
    void encodeNextPacket(ffmpeg::Packet * outPacket, int * nxError);
    void addTimeStamp(int64_t ffmpegPts, int64_t nxTimeStamp);
    int64_t getNxTimeStamp(int64_t ffmpegPts);

    std::shared_ptr<ffmpeg::Frame> newScaledFrame(int * ffmpegErrorCode);

    bool ensureInitialized();
    int initialize();
    void uninitialize();
    int openVideoEncoder();
    int openVideoDecoder();
    void initializeInjector();
    int initializeScaledFrame(const std::shared_ptr<ffmpeg::Codec>& encoder);
    void setEncoderOptions(const std::shared_ptr<ffmpeg::Codec>& encoder);
    
};

} // namespace rpi_cam2
} // namespace nx
