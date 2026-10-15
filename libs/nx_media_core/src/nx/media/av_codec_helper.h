// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

extern "C" {
// For declarations.
#include <libavformat/avformat.h>
}

#include <QtCore/QString>

/**
 * Utilities for ffmpeg data types which do not depend on ffmpeg implementation.
 */
class NX_MEDIA_CORE_API QnAvCodecHelper
{
public:
    /**
     * @return Codec name. Can be empty for invalid/unknown codecs.
     */
    static const QString& codecIdToString(AVCodecID codecId);

    /**
     * Case-insensitive codec ID search by its name.
     * @return Codec ID. AV_CODEC_ID_NONE if not found.
     */
    static AVCodecID codecIdFromString(const QString& codecId);
};
