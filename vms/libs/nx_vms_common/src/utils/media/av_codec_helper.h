// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef AV_CODEC_HELPER_H
#define AV_CODEC_HELPER_H

extern "C"
{
// For declarations.
#include <libavformat/avformat.h>
}

#include <QString>

/**
 * Utilities for ffmpeg data types which do not depend on ffmpeg implementation.
 */
class NX_VMS_COMMON_API QnAvCodecHelper
{
public:
    /**
     * @return Codec name. Can be empty, but never null.
     */
    static QString codecIdToString(AVCodecID codecId);
    static AVCodecID codecIdFromString(const QString& codecId);

    /**
     * Length of AVCodecContext's intra_matrix and inter_matrix; defined by
     * ffmpeg impl.
     */
    static const int kMatrixLength = 64;
};

#endif // AV_CODEC_HELPER_H
