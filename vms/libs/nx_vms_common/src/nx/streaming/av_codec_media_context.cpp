// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "av_codec_media_context.h"

#include <utils/media/ffmpeg_helper.h>
#include <utils/media/av_codec_helper.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

CodecParameters::~CodecParameters()
{
    avcodec_parameters_free(&m_codecParams);
}

CodecParameters::CodecParameters()
{
    m_codecParams = avcodec_parameters_alloc();
    NX_ASSERT(m_codecParams);
}

CodecParameters::CodecParameters(const AVCodecContext* codecContext):
    CodecParameters()
{
    if (codecContext)
        avcodec_parameters_from_context(m_codecParams, codecContext);
}

CodecParameters::CodecParameters(const AVCodecParameters* avCodecParams):
    CodecParameters()
{
    if (avCodecParams)
        avcodec_parameters_copy(m_codecParams, avCodecParams);
}

QByteArray CodecParameters::serialize() const
{
    QnMediaContextSerializableData data;

    data.initializeFrom(m_codecParams);

    return data.serialize();
}

bool CodecParameters::deserialize(const QByteArray& data)
{
    if (data.size() < 4 || data[0] != '[' || data[1] != 'l') //< Ubjson starts with CodecID ('l').
    {
        NX_WARNING(this, "Invalid codec parameters data");
        return false;
    }

    QnMediaContextSerializableData deserializedData;

    if (!deserializedData.deserialize(data))
    {
        NX_WARNING(this, "Failed to deserialize codec parameters data");
        return false;
    }

    m_codecParams->codec_id = deserializedData.codecId;
    m_codecParams->codec_type = deserializedData.codecType;
    m_codecParams->channels = deserializedData.channels;
    m_codecParams->sample_rate = deserializedData.sampleRate;
    m_codecParams->format = deserializedData.sampleFmt;
    m_codecParams->bits_per_coded_sample = deserializedData.bitsPerCodedSample;
    m_codecParams->width = deserializedData.width;
    m_codecParams->height = deserializedData.height;
    m_codecParams->bit_rate = deserializedData.bitRate;
    m_codecParams->channel_layout = deserializedData.channelLayout;
    m_codecParams->block_align = deserializedData.blockAlign;

    setExtradata((uint8_t*)deserializedData.extradata.data(), deserializedData.extradata.size());
    return true;
}

void CodecParameters::toAvCodecContext(AVCodecContext* context) const
{
    if (avcodec_parameters_to_context(context, m_codecParams) != 0)
        NX_WARNING(this, "Failed to copy AVCodecParameters to AVCodecContext");
}

bool CodecParameters::isEqual(const CodecParameters& other) const
{
    if (getCodecId() != other.getCodecId()
        || getCodecType() != other.getCodecType()
        || getWidth() != other.getWidth()
        || getHeight() != other.getHeight()
        || getChannels() != other.getChannels()
        || getChannelLayout() != other.getChannelLayout()
        || getSampleRate() != other.getSampleRate()
        || getSampleFmt() != other.getSampleFmt()
        || getBitsPerCodedSample() != other.getBitsPerCodedSample()
        || getBitRate() != other.getBitRate()
        || getBlockAlign() != other.getBlockAlign()
        || getExtradataSize() != other.getExtradataSize()
        || memcmp(getExtradata(), other.getExtradata(), getExtradataSize()) != 0)
    {
        return false;
    }
    return true;
}

QString CodecParameters::toString() const
{
    return
        NX_FMT("CodecId: %1, CodecType: %2, CodecTag: %3",
            m_codecParams->codec_id, m_codecParams->codec_type, m_codecParams->codec_tag) + " " +
        NX_FMT("Width: %1, Height: %2, Channels: %3, ChannelLayout: %4, "
            "SampleRate: %5, SampleFmt: %6, BitsPerCodedSample: %7, BitRate: %8, BlockAlign: %9, "
            "ExtradataSize: %10",
            getWidth(), getHeight(), getChannels(), getChannelLayout(), getSampleRate(),
            getSampleFmt(), getBitsPerCodedSample(), getBitRate(), getBlockAlign(),
            getExtradataSize());
}

QString CodecParameters::getCodecName() const
{
    return QnAvCodecHelper::codecIdToString(getCodecId());
}

QString CodecParameters::getAudioCodecDescription() const
{
    QString result;
    QString codecStr = QnAvCodecHelper::codecIdToString(getCodecId());
    if (!codecStr.isEmpty())
    {
        result += codecStr;
        result += " ";
    }

    if (getChannels() == 3)
        result += "2.1";
    else if (getChannels() == 6)
        result += "5.1";
    else if (getChannels() == 8)
        result += "7.1";
    else if (getChannels() == 2)
        result += "stereo";
    else if (getChannels() == 1)
        result += "mono";
    else
        result += QString::number(getChannels());

    return result;
}

void CodecParameters::setExtradata(const uint8_t* data, int size)
{
    if (m_codecParams->extradata)
        av_freep(&m_codecParams->extradata);

    if (!data || size == 0)
        return;

    m_codecParams->extradata = (uint8_t*) av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE);
    NX_ASSERT(m_codecParams->extradata, "Failed to allocate extradata: out of memory");

    memcpy(m_codecParams->extradata, data, size);
    m_codecParams->extradata_size = size;
}

//-------------------------------------------------------------------------------------------------

AVCodecID CodecParameters::getCodecId() const
{
    return m_codecParams->codec_id;
}

AVMediaType CodecParameters::getCodecType() const
{
    return m_codecParams->codec_type;
}

const quint8* CodecParameters::getExtradata() const
{
    return m_codecParams->extradata;
}

int CodecParameters::getExtradataSize() const
{
    return m_codecParams->extradata_size;
}

int CodecParameters::getChannels() const
{
    return m_codecParams->channels;
}

int CodecParameters::getSampleRate() const
{
    return m_codecParams->sample_rate;
}

AVSampleFormat CodecParameters::getSampleFmt() const
{
    return (AVSampleFormat)m_codecParams->format;
}

int CodecParameters::getBitsPerCodedSample() const
{
    return m_codecParams->bits_per_coded_sample;
}

int CodecParameters::getWidth() const
{
    return m_codecParams->width;
}

int CodecParameters::getHeight() const
{
    return m_codecParams->height;
}

int CodecParameters::getBitRate() const
{
    return m_codecParams->bit_rate;
}

quint64 CodecParameters::getChannelLayout() const
{
    return m_codecParams->channel_layout;
}

int CodecParameters::getBlockAlign() const
{
    return m_codecParams->block_align;
}

int CodecParameters::getFrameSize() const
{
    return m_codecParams->frame_size;
}
