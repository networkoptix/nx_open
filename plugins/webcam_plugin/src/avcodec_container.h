#pragma once

#include <QtCore/QString>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include "av_string_error.h"

namespace nx {
namespace webcam_plugin {

class AVCodecContainer
{
public:
    AVCodecContainer();
    ~AVCodecContainer();

    /*!
    * Convenience function for setting the bitrate in bits/s before callings open()
    * @param[in] bitrate - the bitrate in bits/s.
    */
    void setBitrate(int bitrate);

    int open();
    int close();

    static int readFrame(AVFormatContext * formatContext, AVPacket * outPacket);

    int encodeVideo(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket) const;
    int decodeVideo(AVFormatContext* formatContext, AVFrame *outFrame, int *outGotPicture, AVPacket *packet) const;

    int encodeAudio(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket) const;
    int decodeAudio(AVFrame * frame, int* outGotFrame, const AVPacket *packet) const;

    void initializeEncoder(AVCodecID codecID);
    void initializeDecoder(AVCodecParameters * codecParameters);
    void initializeDecoder(AVCodecID codecID);
    bool isValid() const;

    AVStringError lastError() const;

    AVCodecContext* codecContext() const;
    AVCodec* codec() const;
    AVCodecID codecID() const;
    AVDictionary* options() const;

    QString avErrorString() const;

private:
    AVCodecContext * m_codecContext;
    AVCodec * m_codec;
    AVDictionary* m_options;

    AVStringError m_lastError;

    bool m_open;
};

} // namespace webcam_plugin
} // namespace nx
