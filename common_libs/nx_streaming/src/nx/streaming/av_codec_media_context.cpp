#include "av_codec_media_context.h"

#include <utils/media/ffmpeg_helper.h>
#include <utils/media/av_codec_helper.h>
#include <nx/utils/log/assert.h>

QnAvCodecMediaContext::~QnAvCodecMediaContext()
{
    QnFfmpegHelper::deleteAvCodecContext(m_context);
    m_context = nullptr;
}

QnAvCodecMediaContext::QnAvCodecMediaContext(AVCodecID codecId):
    m_context(QnFfmpegHelper::createAvCodecContext(codecId))
{
    NX_ASSERT(m_context);
}

QnAvCodecMediaContext::QnAvCodecMediaContext(const AVCodecContext* context):
    m_context(QnFfmpegHelper::createAvCodecContext(context))
{
    NX_ASSERT(m_context);
}

QnAvCodecMediaContext* QnAvCodecMediaContext::cloneWithoutExtradata() const
{
    QnAvCodecMediaContext* newContext = new QnAvCodecMediaContext(m_context);
    newContext->setExtradata(nullptr, 0);
    return newContext;
}

QByteArray QnAvCodecMediaContext::serialize() const
{
    QnMediaContextSerializableData data;

    data.initializeFrom(m_context);

    return data.serialize();
}

void QnAvCodecMediaContext::setExtradata(
    const quint8* extradata, int extradata_size)
{
    NX_ASSERT(!m_context->extradata, Q_FUNC_INFO,
        "Attempt to reassign AVCodecContext extradata.");

    QnFfmpegHelper::copyAvCodecContextField(
        (void**) &m_context->extradata, extradata, extradata_size);
    m_context->extradata_size = extradata_size;
}

//-------------------------------------------------------------------------------------------------

AVCodecID QnAvCodecMediaContext::getCodecId() const
{
    return m_context->codec_id;
}

AVMediaType QnAvCodecMediaContext::getCodecType() const
{
    return m_context->codec_type;
}

const char* QnAvCodecMediaContext::getRcEq() const
{
    return m_context->rc_eq;
}

const quint8* QnAvCodecMediaContext::getExtradata() const
{
    return m_context->extradata;
}

int QnAvCodecMediaContext::getExtradataSize() const
{
    return m_context->extradata_size;
}

const quint16* QnAvCodecMediaContext::getIntraMatrix() const
{
    return m_context->intra_matrix;
}

const quint16* QnAvCodecMediaContext::getInterMatrix() const
{
    return m_context->inter_matrix;
}

const RcOverride* QnAvCodecMediaContext::getRcOverride() const
{
    return m_context->rc_override;
}

int QnAvCodecMediaContext::getRcOverrideCount() const
{
    return m_context->rc_override_count;
}

int QnAvCodecMediaContext::getChannels() const
{
    return m_context->channels;
}

int QnAvCodecMediaContext::getSampleRate() const
{
    return m_context->sample_rate;
}

AVSampleFormat QnAvCodecMediaContext::getSampleFmt() const
{
    return m_context->sample_fmt;
}

int QnAvCodecMediaContext::getBitsPerCodedSample() const
{
    return m_context->bits_per_coded_sample;
}

int QnAvCodecMediaContext::getCodedWidth() const
{
    return m_context->coded_width;
}

int QnAvCodecMediaContext::getCodedHeight() const
{
    return m_context->coded_height;
}

int QnAvCodecMediaContext::getWidth() const
{
    return m_context->width;
}

int QnAvCodecMediaContext::getHeight() const
{
    return m_context->height;
}

int QnAvCodecMediaContext::getBitRate() const
{
    return m_context->bit_rate;
}

quint64 QnAvCodecMediaContext::getChannelLayout() const
{
    return m_context->channel_layout;
}

int QnAvCodecMediaContext::getBlockAlign() const
{
    return m_context->block_align;
}

int QnAvCodecMediaContext::getFrameSize() const
{
    return m_context->frame_size;
}
