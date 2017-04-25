#pragma once

#include <cstdint>

#include <QtCore/QString>
#include <QtCore/QIODevice>

#include <core/resource/resource_media_layout.h>

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

uint64_t mediaDurationMs(QIODevice* device);

QnResourceAudioLayoutPtr audioLayout(QIODevice* inputMedia);

Error remux(QIODevice* inputMedia, QIODevice* outMedia, const QString& dstContainer);


} // namespace transcoding
} // namespace nx