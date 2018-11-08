/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#include "audio_data_packet.h"

////////////////////////////////////////////////////////////
//// class QnCodecAudioFormat
////////////////////////////////////////////////////////////

QnCodecAudioFormat::QnCodecAudioFormat()
:
    QnAudioFormat(),
    bitrate(0),
    channel_layout(0),
    block_align(0),
    m_bitsPerSample(0)
    //m_frequency(0)
{
    // Do nothing.
}

QnCodecAudioFormat::QnCodecAudioFormat(const QnConstMediaContextPtr& c)
{
    if (c == nullptr)
        return;

    if (c->getSampleRate())
        setSampleRate(c->getSampleRate());

    if (c->getChannels())
        setChannelCount(c->getChannels());

    //setCodec("audio/pcm");
    setByteOrder(QnAudioFormat::LittleEndian);

    switch(c->getSampleFmt())
    {
        case AV_SAMPLE_FMT_U8: ///< unsigned 8 bits
        case AV_SAMPLE_FMT_U8P:
            setSampleSize(8);
            setSampleType(QnAudioFormat::UnSignedInt);
            break;

        case AV_SAMPLE_FMT_S16: ///< signed 16 bits
        case AV_SAMPLE_FMT_S16P:
            setSampleSize(16);
            setSampleType(QnAudioFormat::SignedInt);
            break;

        case AV_SAMPLE_FMT_S32:///< signed 32 bits
        case AV_SAMPLE_FMT_S32P:
            setSampleSize(32);
            setSampleType(QnAudioFormat::SignedInt);
            break;

        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            setSampleSize(32);
            setSampleType(QnAudioFormat::Float);
            break;

        default:
            setSampleSize(16);
            setSampleType(QnAudioFormat::SignedInt);
            break;
    }

    if (c->getExtradataSize() > 0)
    {
        extraData.resize(c->getExtradataSize());
        memcpy(&extraData[0], c->getExtradata(), c->getExtradataSize());
    }
    bitrate = c->getBitRate();
    channel_layout = c->getChannelLayout();
    block_align = c->getBlockAlign();
    m_bitsPerSample = c->getBitsPerCodedSample();
}

////////////////////////////////////////////////////////////
//// class QnCompressedAudioData
////////////////////////////////////////////////////////////

QnCompressedAudioData::QnCompressedAudioData(const QnConstMediaContextPtr& ctx)
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
    unsigned int alignment,
    unsigned int capacity,
    QnConstMediaContextPtr ctx )
:   //TODO #ak delegate constructor
    QnCompressedAudioData( ctx ),
    m_data( alignment, capacity )
{
}

QnWritableCompressedAudioData::QnWritableCompressedAudioData(
    QnAbstractAllocator* allocator,
    unsigned int alignment,
    unsigned int capacity,
    QnConstMediaContextPtr ctx )
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
