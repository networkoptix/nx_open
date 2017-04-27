#pragma once

#include <cstdint>

#include <QtCore/QString>
#include <QtCore/QIODevice>

#include <core/resource/resource_media_layout.h>

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

} // namespace transcoding
} // namespace nx