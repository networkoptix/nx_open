#pragma once

#include <cstdint>

#include <QtCore/QString>
#include <QtCore/QIODevice>

#include <core/resource/resource_media_layout.h>
#include <nx/streaming/video_data_packet.h>

extern "C" {
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
}

namespace nx {
namespace transcoding {

enum class Error
{
    noError,
    unknown,
    noStreamInfo,
    unknownFormat,
    unableToOpenInput,
    badAlloc,
    noEncoder,
    noDecoder,
    failedToWriteHeader
};

Error remux(QIODevice* inputMedia, QIODevice* outMedia, const QString& dstContainer);

class Helper
{
public:
    Helper(QIODevice* inputMedia);
    ~Helper();

    bool open();
    void close();

    int64_t durationMs();
    QnResourceAudioLayoutPtr audioLayout();
    QSize resolution();

private:
    QIODevice* m_inputMedia;
    AVIOContext* m_inputAvioContext;
    AVFormatContext* m_inputFormatContext;
};

AVCodecID findEncoderCodecId(const QString& codecName);

QSize normalizeResolution(const QSize& target, const QSize& source);
QSize adjustCodecRestrictions(AVCodecID codec, const QSize& source);
QSize cropResolution(const QSize& source, const QSize& max);
QSize downscaleByHeight(const QSize& source, int newHeight);
QSize downscaleByWidth(const QSize& source, int newWidth);
QSize findMaxSavedResolution(const QnConstCompressedVideoDataPtr& video);

inline const char* ffmpegErrorText(const int error)
{
    static char error_buffer[255];
    av_strerror(error, error_buffer, sizeof(error_buffer));
    return error_buffer;
}

} // namespace transcoding
} // namespace nx
