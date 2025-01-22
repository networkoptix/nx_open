// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transcoder.h"

#include <QtCore/QThread>

#include <core/resource/media_resource.h>
#include <nx/media/config.h>
#include <nx/media/sse_helper.h>
#include <nx/metric/metrics_storage.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <transcoding/ffmpeg_transcoder.h>
#include <transcoding/ffmpeg_video_transcoder.h>


// ---------------------------- QnCodecTranscoder ------------------
QnCodecTranscoder::QnCodecTranscoder(AVCodecID codecId)
:
    m_bitrate(-1),
    m_quality(Qn::StreamQuality::normal)
{
    m_codecId = codecId;
}

void QnCodecTranscoder::setParams(const QnCodecParams::Value& params)
{
    m_params = params;
}

void QnCodecTranscoder::setBitrate(int value)
{
    m_bitrate = value;
}

int QnCodecTranscoder::getBitrate() const
{
    return m_bitrate;
}

/*
AVCodecContext* QnCodecTranscoder::getCodecContext()
{
    return 0;
}
*/

void QnCodecTranscoder::setQuality( Qn::StreamQuality quality )
{
    m_quality = quality;
}

QRect QnCodecTranscoder::roundRect(const QRect& srcRect)
{
    int left = qPower2Floor((unsigned) srcRect.left(), CL_MEDIA_ALIGNMENT);
    int top = qPower2Floor((unsigned) srcRect.top(), 2);

    int width = qPower2Ceil((unsigned) srcRect.width(), 16);
    int height = qPower2Ceil((unsigned) srcRect.height(), 2);

    return QRect(left, top, width, height);
}

QSize QnCodecTranscoder::roundSize(const QSize& size)
{
    int width = qPower2Ceil((unsigned) size.width(), 16);
    int height = qPower2Ceil((unsigned) size.height(), 4);

    return QSize(width, height);
}

// --------------------------- QnVideoTranscoder -----------------
QnVideoTranscoder::QnVideoTranscoder(AVCodecID codecId):
    QnCodecTranscoder(codecId)
{
}
