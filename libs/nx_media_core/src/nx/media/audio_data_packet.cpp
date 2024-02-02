// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_data_packet.h"

#include <nx/build_info.h>
#include <nx/utils/log/assert.h>

////////////////////////////////////////////////////////////
//// class QnCompressedAudioData
////////////////////////////////////////////////////////////

QnCompressedAudioData::QnCompressedAudioData(const CodecParametersConstPtr& ctx)
:
    QnAbstractMediaData( AUDIO ),
    duration( 0 )
{
    context = ctx;
}

void QnCompressedAudioData::assign(const QnCompressedAudioData* other)
{
    QnAbstractMediaData::assign(other);
    duration = other->duration;
}

quint64 QnCompressedAudioData::getDurationMs() const
{
    if (!context)
        return 0;

    if (context->getSampleRate())
        return (quint64)((context->getFrameSize() / (double)context->getSampleRate()) * 1000);
    else
        return 0;
}


////////////////////////////////////////////////////////////
//// class QnWritableCompressedAudioData
////////////////////////////////////////////////////////////

QnWritableCompressedAudioData::QnWritableCompressedAudioData(
    size_t capacity,
    CodecParametersConstPtr ctx)
    :
    QnCompressedAudioData(ctx),
    m_data(CL_MEDIA_ALIGNMENT, capacity, AV_INPUT_BUFFER_PADDING_SIZE)
{
}

QnWritableCompressedAudioData* QnWritableCompressedAudioData::clone() const
{
    QnWritableCompressedAudioData* rez = new QnWritableCompressedAudioData();
    rez->assign(this);
    return rez;
}

const char* QnWritableCompressedAudioData::data() const
{
    return m_data.data();
}

size_t QnWritableCompressedAudioData::dataSize() const
{
    return m_data.size();
}

void QnWritableCompressedAudioData::setData(nx::utils::ByteArray&& buffer)
{
    m_data = std::move(buffer);
}

void QnWritableCompressedAudioData::assign( const QnWritableCompressedAudioData* other )
{
    QnCompressedAudioData::assign( other );
    m_data = other->m_data;
}
