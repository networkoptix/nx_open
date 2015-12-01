#ifndef CODEC_HELPER_H
#define CODEC_HELPER_H

// Utilities for ffmpeg data types which do not depend on ffmpeg implementation.

// TODO mike: Decide on unit name.

#include <QString>

extern "C"
{
// For enum CodecID, struct AVCodecContext.
#include <libavformat/avformat.h>
}

#include <core/datapacket/media_context.h>

QString codecIDToString(CodecID codecId);

QString getAudioCodecDescription(const QnConstMediaContextPtr& context);

/**
 * @param av Should be already allocated but not filled yet.
 */
void mediaContextToAvCodecContext(AVCodecContext* av, const QnMediaContext* media);

#endif // CODEC_HELPER_H
