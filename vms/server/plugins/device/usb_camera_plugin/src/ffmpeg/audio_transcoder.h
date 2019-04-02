#pragma once

#include <memory>
#include <chrono>

#include "packet.h"
#include "codec.h"
#include "frame.h"

struct SwrContext;
struct AVStream;

namespace nx::usb_cam::ffmpeg {

class AudioTranscoder
{
public:
    int initialize(AVStream* stream);
    void uninitialize();
    int sendPacket(const ffmpeg::Packet& packet);
    int receivePacket(std::shared_ptr<ffmpeg::Packet>& result);
    int targetSampleRate() { return m_encoder->sampleRate(); }
    AVCodecContext* getCodecContext();

private:
    int initializeEncoder();
    int initializeDecoder(AVStream* stream);
    int initializeResampledFrame();
    int initalizeResampleContext(const AVFrame* frame);
    int decodeNextFrame(ffmpeg::Frame* outFrame);
    int resample(const AVFrame* frame, AVFrame* outFrame);
    int encode(const ffmpeg::Frame* frame, ffmpeg::Packet* outPacket);
    bool isFrameCached() const;

private:
    std::unique_ptr<ffmpeg::Codec> m_decoder;
    std::unique_ptr<ffmpeg::Codec> m_encoder;
    std::unique_ptr<ffmpeg::Frame> m_decodedFrame;
    std::unique_ptr<ffmpeg::Frame> m_resampledFrame;

    struct SwrContext * m_resampleContext = nullptr;
};

} // namespace nx::usb_cam::ffmpeg
