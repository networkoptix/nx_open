#pragma once
#include <atomic>

#include "stream_reader.h"
#include "ffmpeg/video_transcoder.h"

namespace nx::usb_cam::ffmpeg { class Codec; }
namespace nx::usb_cam::ffmpeg { class Frame; }

namespace nx::usb_cam {

class TranscodeStreamReader: public StreamReaderPrivate
{
public:
    TranscodeStreamReader(int encoderIndex, const std::shared_ptr<Camera>& camera);
    virtual ~TranscodeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** lpPacket ) override;

    virtual void setFps(float fps) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;

private:
    std::shared_ptr<ffmpeg::Packet> nextPacket();
    int initializeTranscoder();

private:
    ffmpeg::VideoTranscoder m_transcoder;
    CodecParameters m_codecParams;
    std::atomic_bool m_encoderNeedsReinitialization = true;
};

} // namespace usb_cam::nx
