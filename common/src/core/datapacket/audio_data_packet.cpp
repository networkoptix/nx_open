/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#include "audio_data_packet.h"

#ifdef ENABLE_DATA_PROVIDERS

////////////////////////////////////////////////////////////
//// class QnCodecAudioFormat
////////////////////////////////////////////////////////////

QnCodecAudioFormat::QnCodecAudioFormat()
:
    QnAudioFormat(),
    bitrate(0),
    channel_layout(0),
    block_align(0),
    m_bitsPerSample(0),
    m_frequency(0)
{
}

QnCodecAudioFormat::QnCodecAudioFormat(QnMediaContextPtr ctx)
{
    fromAvStream(ctx);
}

QnCodecAudioFormat& QnCodecAudioFormat::fromAvStream(QnMediaContextPtr ctx)
{
    fromAvStream(ctx->ctx());
    return *this;
}

void QnCodecAudioFormat::fromAvStream(AVCodecContext* c)
{
    if (c->sample_rate)
        setSampleRate(c->sample_rate);

    if (c->channels) 
        setChannelCount(c->channels);

    //setCodec("audio/pcm");
    setByteOrder(QnAudioFormat::LittleEndian);

    switch(c->sample_fmt)
    {
    case AV_SAMPLE_FMT_U8: ///< unsigned 8 bits
        setSampleSize(8);
        setSampleType(QnAudioFormat::UnSignedInt);
        break;

    case AV_SAMPLE_FMT_S16: ///< signed 16 bits
        setSampleSize(16);
        setSampleType(QnAudioFormat::SignedInt);
        break;

    case AV_SAMPLE_FMT_S32:///< signed 32 bits
        setSampleSize(32);
        setSampleType(QnAudioFormat::SignedInt);
        break;

    case AV_SAMPLE_FMT_FLT:
        setSampleSize(32);
        setSampleType(QnAudioFormat::Float);
        break;

    default:
        break;
    }

    if (c->extradata_size > 0)
    {
        extraData.resize(c->extradata_size);
        memcpy(&extraData[0], c->extradata, c->extradata_size);
    }
    bitrate = c->bit_rate;
    channel_layout = c->channel_layout;
    block_align = c->block_align;
    m_bitsPerSample = c->bits_per_coded_sample;
}



////////////////////////////////////////////////////////////
//// class QnCompressedAudioData
////////////////////////////////////////////////////////////

QnCompressedAudioData::QnCompressedAudioData( QnMediaContextPtr ctx )
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


////////////////////////////////////////////////////////////
//// class QnWritableCompressedAudioData
////////////////////////////////////////////////////////////

QnWritableCompressedAudioData::QnWritableCompressedAudioData(
    unsigned int alignment,
    unsigned int capacity,
    QnMediaContextPtr ctx )
:   //TODO #ak delegate constructor
    QnCompressedAudioData( ctx ),
    m_data( alignment, capacity )
{
}

QnWritableCompressedAudioData::QnWritableCompressedAudioData(
    QnAbstractAllocator* allocator,
    unsigned int alignment,
    unsigned int capacity,
    QnMediaContextPtr ctx )
:
    QnCompressedAudioData( ctx ),
    m_data( allocator, alignment, capacity )
{
}

QnWritableCompressedAudioData* QnWritableCompressedAudioData::clone( QnAbstractAllocator* allocator ) const
{
    QnWritableCompressedAudioData* rez = new QnWritableCompressedAudioData(
        allocator,
        m_data.getAlignment(),
        m_data.size() );
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

void QnWritableCompressedAudioData::assign( const QnWritableCompressedAudioData* other )
{
    QnCompressedAudioData::assign( other );
    m_data = other->m_data;
}

#endif // ENABLE_DATA_PROVIDERS
