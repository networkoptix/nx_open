// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "codec_parameters.h"

#include <nx/media/av_codec_helper.h>
#include <nx/media/media_context_serializable_data.h>
#include <nx/media/media_context_serializable_data_4_2.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace {

constexpr char kCodecContextVersion = 2;

AVCodecID convertCodecIdFromFfmpeg3_1(AVCodecID id)
{
    if ((id > AV_CODEC_ID_MPEG2VIDEO && id < AV_CODEC_ID_Y41P)
        || (id > AV_CODEC_ID_ATRAC3 && id < AV_CODEC_ID_FFWAVESYNTH))
    {
        return (AVCodecID) ((int) id - 1);
    }
    return id;
}

template<typename T>
bool deserializeCodecParameters(CodecParameters& codecParameters, const char* data, int size)
{
    if (size < 4 || data[0] != '[' || data[1] != 'l') //< Ubjson starts with CodecID ('l').
        return false;

    auto codecParams = codecParameters.getAvCodecParameters();
    T deserializedData;
    if (!deserializedData.deserialize(QByteArray((const char*)data, size)))
        return false;

    codecParams->codec_type = deserializedData.codecType;
    codecParams->channels = deserializedData.channels;
    codecParams->sample_rate = deserializedData.sampleRate;
    codecParams->format = deserializedData.sampleFmt;
    codecParams->bits_per_coded_sample = deserializedData.bitsPerCodedSample;
    codecParams->width = deserializedData.width;
    codecParams->height = deserializedData.height;
    codecParams->bit_rate = deserializedData.bitRate;
    codecParams->channel_layout = deserializedData.channelLayout;
    codecParams->block_align = deserializedData.blockAlign;
    codecParams->codec_id = deserializedData.codecId;
    codecParameters.setExtradata(
        (uint8_t*)deserializedData.extradata.data(), deserializedData.extradata.size());
    return true;
}

}

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

int CodecParameters::version() const
{
    return m_version;
}

QByteArray CodecParameters::serialize() const
{
    QnMediaContextSerializableData data;

    data.initializeFrom(m_codecParams);

    auto result = data.serialize();
    result.prepend(kCodecContextVersion);
    return result;
}

QByteArray CodecParameters::serializeInDeprecatedFormat42() const
{
    QnMediaContextSerializableData_4_2 data;
    data.initializeFrom(m_codecParams);
    auto result = data.serialize();
    return result;
}

bool CodecParameters::deserialize(const char* data, int size, int version)
{
    m_version = version;
    if (size > 0 && data[0] != '[') //< vms_4.2 format starts from ubjson without version
    {
        m_version = data[0];
        ++data;
        --size;
    }

    if (m_version == 0)
    {
        // TODO #lbusygin: get rid of this version when the mobile client stops supporting servers < 5.0
        if (!::deserializeCodecParameters<QnMediaContextSerializableData_4_2>(*this, data, size))
        {
            NX_WARNING(this, "Failed to deserialize codec parameters data(version 4.2)");
            return false;
        }
        m_codecParams->codec_id = convertCodecIdFromFfmpeg3_1(m_codecParams->codec_id);
    }
    else if (m_version == 1)
    {
        if (!::deserializeCodecParameters<QnMediaContextSerializableData_4_2>(*this, data, size))
        {
            NX_WARNING(this, "Failed to deserialize codec parameters data");
            return false;
        }
    }
    else if (m_version >= 2)
    {
        if (m_version > 2)
            NX_DEBUG(this, "Try to deserialize data from the future version");
        if (!::deserializeCodecParameters<QnMediaContextSerializableData>(*this, data, size))
        {
            NX_WARNING(this, "Failed to deserialize codec parameters data");
            return false;
        }
    }
    else
    {
        NX_WARNING(this, "Failed to deserialize codec parameters data, version not supported: %1",
            m_version);
        return false;
    }
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
