#include "media_data_packet.h"

extern "C"
{
    #include <libavformat/avformat.h>
}

#include "utils/media/bitStream.h"
#include "utils/media/ffmpeg_helper.h"
#include "utils/media/sse_helper.h"
#include "utils/common/synctime.h"

#include <QtGui/QRegion>

#ifdef Q_OS_MAC
#include <smmintrin.h>
#endif

#include <utils/math/math.h>

#include "utils/media/sse_helper.h"


QnMediaContext::QnMediaContext(AVCodecContext* ctx)
{
    m_ctx = avcodec_alloc_context3(NULL);
    avcodec_copy_context(m_ctx, ctx);
}

QnMediaContext::QnMediaContext(CodecID codecId)
{
    if (codecId != CODEC_ID_NONE)
    {
        AVCodec* codec = avcodec_find_decoder(codecId);
        if( codec && codec->id == codecId )
        {
            m_ctx = avcodec_alloc_context3(codec);
            avcodec_open2(m_ctx, codec, NULL);
        }
        else
        {
            AVCodec* codec = (AVCodec*)av_malloc(sizeof(AVCodec));
            memset( codec, 0, sizeof(*codec) );
            codec->id = codecId;
            codec->type = AVMEDIA_TYPE_VIDEO;
            m_ctx = avcodec_alloc_context3(codec);
            m_ctx->codec_id = codecId;
        }
    } else {
        m_ctx = 0;
    }
}

QnMediaContext::QnMediaContext(const QByteArray& payload)
{
    m_ctx = QnFfmpegHelper::deserializeCodecContext(payload.data(), payload.size());
}

QnMediaContext::QnMediaContext(const quint8* payload, int dataSize)
{
    m_ctx = QnFfmpegHelper::deserializeCodecContext((const char*) payload, dataSize);
}

QnMediaContext::~QnMediaContext()
{
    if (m_ctx) {
        if (m_ctx->codec)
            avcodec_close(m_ctx);
        av_freep(&m_ctx->rc_override);
        av_freep(&m_ctx->intra_matrix);
        av_freep(&m_ctx->inter_matrix);
        av_freep(&m_ctx->extradata);
        av_freep(&m_ctx->rc_eq);
        av_freep(&m_ctx);
    }
}


AVCodecContext* QnMediaContext::ctx() const
{
    return m_ctx;
}

QString QnMediaContext::codecName() const 
{
    return m_ctx ? codecIDToString(m_ctx->codec_id) : QString();
}

bool QnMediaContext::equalTo(QnMediaContext *other) const
{
    // I've add new condition bits_per_coded_sample for G726 audio codec
    if( m_ctx == NULL && other->m_ctx == NULL )
        return true;
    if( m_ctx == NULL || other->m_ctx == NULL )
        return false;
    if (m_ctx->width != other->m_ctx->width || m_ctx->height != other->m_ctx->height)
        return false;
    
    return m_ctx->codec_id == other->m_ctx->codec_id && m_ctx->bits_per_coded_sample == other->m_ctx->bits_per_coded_sample;
}


void QnAbstractMediaData::assign(const QnAbstractMediaData* other)
{
    dataProvider = other->dataProvider;
    timestamp = other->timestamp;

    data.write(other->data.data(), other->data.size());
    dataType = other->dataType;
    compressionType = other->compressionType;
    flags = other->flags;
    channelNumber = other->channelNumber;
    context = other->context;
    opaque = other->opaque;
}

// ----------------------------------- QnAbstractMediaData -----------------------------------------

QnAbstractMediaData* QnAbstractMediaData::clone() const
{
    QnAbstractMediaData* rez = new QnAbstractMediaData(data.getAlignment(), data.size());
    rez->assign(this);
    return rez;
}

// ----------------------------------- QnCompressedVideoData -----------------------------------------
void QnCompressedVideoData::assign(const QnCompressedVideoData* other)
{
    QnAbstractMediaData::assign(other);
    width = other->width;
    height = other->height;
    motion = other->motion;
    pts = other->pts;
}

QnCompressedVideoData* QnCompressedVideoData::clone() const
{
    QnCompressedVideoData* rez = new QnCompressedVideoData(data.getAlignment(), data.size());
    rez->assign(this);
    return rez;
}

// ----------------------------------- QnMetaDataV1 -----------------------------------------

QnMetaDataV1::QnMetaDataV1(int initialValue):
    QnAbstractMediaData(CL_MEDIA_ALIGNMENT, MD_WIDTH*MD_HEIGHT/8)
{
    dataType = META_V1;
    //useTwice = false;

    flags = 0;
    m_input = 0;
    m_duration = 0;
    m_firstTimestamp = AV_NOPTS_VALUE;
    timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;
    if (initialValue)
        data.writeFiller(0xff, data.capacity());
    else
        data.writeFiller(0, data.capacity());
}

QnMetaDataV1Ptr QnMetaDataV1::fromLightData(const QnMetaDataV1Light& lightData)
{
    QnMetaDataV1Ptr result(new QnMetaDataV1());
    result->timestamp = lightData.startTimeMs*1000;
    result->m_duration = lightData.durationMs*1000;
    result->channelNumber = lightData.channel;
    result->m_input = lightData.input;
    memcpy(result->data.data(), lightData.data, result->data.size());
    return result;
}

#if defined(__i386) || defined(__amd64) || defined(_WIN32)
inline bool sse4_attribute mathImage_sse41(const __m128i* data, const __m128i* mask, int maskStart, int maskEnd)
{
    for (int i = maskStart; i <= maskEnd; ++i)
    {
        if (_mm_testz_si128(mask[i], data[i]) == 0) /* SSE4. */
            return true;
    }
    return false;
}

inline bool mathImage_sse2(const __m128i* data, const __m128i* mask, int maskStart, int maskEnd)
{
    static const __m128i zerroValue = _mm_setr_epi32(0, 0, 0, 0);
    for (int i = maskStart; i <= maskEnd; ++i)
    {
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(_mm_and_si128(mask[i], data[i]), zerroValue)) != 0xffff) /* SSE2. */
            return true;
    }
    return false;
}
#endif    //__i386

inline bool mathImage_cpu(const simd128i* data, const simd128i* mask, int maskStart, int maskEnd)
{
    uint64_t* curPtr = (uint64_t*) data;
    curPtr += maskStart*2;
    uint64_t* maskPtr = (uint64_t*) mask; 
    maskPtr += maskStart*2;

    for (int i = maskStart; i <= maskEnd; ++i)
    {
        if (*curPtr++ & *maskPtr++)
            return true;
        if (*curPtr++ & *maskPtr++)
            return true;
    }
    return false;
}

bool QnMetaDataV1::mathImage(const simd128i* data, const simd128i* mask, int maskStart, int maskEnd)
{
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    if (useSSE41())
        return mathImage_sse41(data, mask, maskStart, maskEnd);
    else 
        return mathImage_sse2(data, mask, maskStart, maskEnd);
#elif __arm__ && __ARM_NEON__
    //TODO/ARM
    return mathImage_cpu(data, mask, maskStart, maskEnd);
#else
    return mathImage_cpu(data, mask, maskStart, maskEnd);
#endif
}


void QnMetaDataV1::assign(const QnMetaDataV1* other)
{
    QnAbstractMediaData::assign(other);
    m_input = other->m_input;
    m_duration = other->m_duration;
    m_firstTimestamp = other->m_firstTimestamp;
}

QnMetaDataV1* QnMetaDataV1::clone() const
{
    QnMetaDataV1* rez = new QnMetaDataV1();
    rez->assign(this);
    return rez;
}

void QnMetaDataV1::addMotion(QnConstMetaDataV1Ptr data)
{
    addMotion((const quint8*) data->data.data(), data->timestamp);
}

void QnMetaDataV1::addMotion(QnMetaDataV1Ptr data)
{
    addMotion((const quint8*) data->data.data(), data->timestamp);
}

void QnMetaDataV1::removeMotion(const simd128i* image, int startIndex, int endIndex)
{
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    __m128i* dst = (__m128i*) data.data();
    __m128i* src = (__m128i*) image;
    for (int i = startIndex; i <= endIndex; ++i)
    {
        *dst = _mm_andnot_si128(*src, *dst); /* SSE2. */
        dst++;
        src++;

    }
#else
    // remove without SIMD
    int64_t* dst = (int64_t*) data.data();
    int64_t* src = (int64_t*) image;
    for (int i = startIndex; i <= endIndex; ++i)
    {
        *dst++ &= *src++;
        *dst++ &= *src++;
    }
#endif
}

#if defined(__i386) || defined(__amd64) || defined(_WIN32)
inline bool metadataIsEmpty_sse2(__m128i* src)
{
    static const __m128i zerroValue = _mm_setr_epi32(0, 0, 0, 0); /* SSE2. */
    for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
    {
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(src[i], zerroValue)) != 0xffff) /* SSE2. */
            return false;
    }
    return true;
}

inline bool sse4_attribute metadataIsEmpty_sse41(__m128i* src)
{
    static const __m128i ff_mask = _mm_setr_epi32(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff); /* SSE2. */
    for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
    {
        if (_mm_testz_si128(src[i], ff_mask) == 0) /* SSE4. */
            return false;
    }
    return true;
}
#endif

inline bool metadataIsEmpty_cpu(const char* data)
{
    const quint32* curPtr = (const quint32*) data;
    for (int i = 0; i < MD_WIDTH*MD_HEIGHT/sizeof(quint32)/CHAR_BIT; ++i)
    {
        if (*curPtr++)
            return false;
    }
    return true;
}

bool QnMetaDataV1::isEmpty() const
{
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    if (useSSE41())
        return metadataIsEmpty_sse41((__m128i*) data.data());
    else 
        return metadataIsEmpty_sse2((__m128i*) data.data());
#elif __arm__ && __ARM_NEON__
    return metadataIsEmpty_cpu(data.data());
#else
    return metadataIsEmpty_cpu(data.data());
#endif
}

void QnMetaDataV1::assign( const nxcip::Picture& motionPicture, qint64 timestamp, qint64 duration )
{
    if( motionPicture.pixelFormat() != nxcip::PIX_FMT_MONOBLACK )
        return;

#if 1
    assert( motionPicture.width() == MD_HEIGHT && motionPicture.height() == MD_WIDTH );

    if( motionPicture.xStride(0)*CHAR_BIT == motionPicture.width() )
        memcpy( data.data(), motionPicture.data(), MD_WIDTH*MD_HEIGHT/CHAR_BIT );
    else
        assert( false );
#else
    memset( data.data(), 0, data.size() );

    //TODO/IMPL some optimization would be appropriate, but is difficult, 
        //since data has following format:
            //column1
            //column2
            //.. columnN

    for( int y = 0; y < std::min<int>(motionPicture.height(), MD_HEIGHT); ++y )
    {
        const quint8* srcMotionDataLine = (quint8*)motionPicture.scanLine( 0, y );
        for( int x = 0; x < std::min<int>(motionPicture.width(), MD_WIDTH); ++x )
        {
            int pixel = *(srcMotionDataLine + x/CHAR_BIT) & (1 << (x%CHAR_BIT));
            if( pixel )
                setMotionAt( x, y );
        }
    }
#endif

    m_firstTimestamp = timestamp;
    m_duration = duration;
}

void QnMetaDataV1::addMotion(const quint8* image, qint64 timestamp)
{
    if ((quint64)m_firstTimestamp == AV_NOPTS_VALUE)
        m_firstTimestamp = timestamp;
    else 
        m_duration = qMax(m_duration, timestamp - m_firstTimestamp);

#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    __m128i* dst = (__m128i*) data.data();
    __m128i* src = (__m128i*) image;
    for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
    {
        *dst = _mm_or_si128(*dst, *src); /* SSE2. */
        dst++;
        src++;

    }
#else
    // remove without SIMD
    int64_t* dst = (int64_t*) data.data();
    int64_t* src = (int64_t*) image;
    for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
    {
        *dst++ |= *src++;
        *dst++ |= *src++;
    }
#endif
}

bool QnMetaDataV1::isMotionAt(int x, int y, char* mask)
{
    Q_ASSERT(x<MD_WIDTH);
    Q_ASSERT(y<MD_HEIGHT);

    int shift = x*MD_HEIGHT + y;
    unsigned char b = *((unsigned char*)mask + shift/8 );
    return b & (128 >> (shift&7));
}

bool QnMetaDataV1::isMotionAt(int x, int y) const
{
    Q_ASSERT(x<MD_WIDTH);
    Q_ASSERT(y<MD_HEIGHT);

    int shift = x*MD_HEIGHT + y;
    unsigned char b = *((unsigned char*)data.data() + shift/8 );
    return b & (128 >> (shift&7));
}

void QnMetaDataV1::setMotionAt(int x, int y) 
{
    Q_ASSERT(x<MD_WIDTH);
    Q_ASSERT(y<MD_HEIGHT);

    int shift = x*MD_HEIGHT + y;
    quint8* b = (quint8*)data.data() + shift/8;
    *b |= (128 >> (shift&7));
}


bool QnMetaDataV1::mapMotion(const QRect& imageRect, const QRect& mRect)
{
    QRect motioRect = imageRect.intersected(mRect);

    if (motioRect.isNull())
        return false;

    //int localZoneWidth = imageRect.width() / MD_WIDTH;
    //int localZoneHight = imageRect.height() / MD_HEIGHT;

    int leftZone = motioRect.left() * MD_WIDTH / imageRect.width();
    int topZone = motioRect.top() * MD_HEIGHT / imageRect.height();

    int rightZone = motioRect.right() * MD_WIDTH / imageRect.width();
    int bottomZone = motioRect.bottom() * MD_HEIGHT / imageRect.height();

    for (int x = leftZone; x <= rightZone; ++x) {
        for (int y = topZone; y <= bottomZone; ++y)
        {
            setMotionAt(x, y);
        }
    }
    return true;
}

bool QnMetaDataV1::containTime(const qint64 timeUsec) const
{
    if (m_duration == 0)
        return timestamp == timeUsec;
    else if( timestamp <= timeUsec && timestamp + m_duration > timeUsec )
        return true;
    else
        return false;
}

inline void setBit(quint8* data, int x, int y)
{
    bool correctData = (x >= 0 && x < MD_WIDTH) && (y >= 0 && y < MD_HEIGHT);

    Q_ASSERT(correctData);

    if (!correctData)
        return;

    int offset = (x * MD_HEIGHT + y) / 8;
    data[offset] |= 0x80 >> (y&7);
}


void QnMetaDataV1::createMask(const QRegion& region,  char* mask, int* maskStart, int* maskEnd)
{
    if (maskStart)
        *maskStart = 0;
    if (maskEnd)
        *maskEnd = 0;
    memset(mask, 0, MD_WIDTH * MD_HEIGHT / 8);

    for (int i = 0; i < region.rectCount(); ++i)
    {
        const QRect& rect = region.rects().at(i);
        if (maskStart)
            *maskStart = qMin((rect.left() * MD_HEIGHT + rect.top()) / 128, *maskStart);
        if (maskEnd)
            *maskEnd = qMax((rect.right() * MD_HEIGHT + rect.bottom()) / 128, *maskEnd);
        for (int x = rect.left(); x <= rect.right(); ++x)
        {
            for (int y = rect.top(); y <= rect.bottom(); ++y)
            {
                setBit((quint8*) mask, x,y);
            }
        }
    }

}

void QnMetaDataV1::serialize(QIODevice* ioDevice) const
{
    Q_ASSERT(channelNumber <= 255);
    qint64 timeStampMs = htonll(timestamp/1000);
    int durationMs = htonl(m_duration/1000);
    ioDevice->write((const char*) &timeStampMs, sizeof(qint64));
    ioDevice->write((const char*) &durationMs, sizeof(int));
    quint8 channel8 = channelNumber;
    ioDevice->write((const char*) &channel8, 1);
    ioDevice->write((const char*) &m_input, 1);
    quint16 reserved  = 0;
    ioDevice->write((const char*) &reserved, 2);
    ioDevice->write(data.data(), data.size());
}

/*
void QnMetaDataV1::deserialize(QIODevice* ioDevice)
{
    qint64 timeMs = 0;
    int durationMs = 0;

    ioDevice->read((char*) &timeMs, sizeof(timeMs));
    ioDevice->read((char*) &durationMs, 3);
    ioDevice->read((char*) &m_input, 1);
    ioDevice->read(data.data(), data.size());

    timeMs = ntohll(timeMs);
    durationMs = ntohl(durationMs);
}
*/

bool operator< (const QnMetaDataV1Light& data, const quint64 timeMs)
{
    return data.startTimeMs < timeMs;
}

bool operator< (const quint64 timeMs, const QnMetaDataV1Light& data)
{
    return timeMs < data.startTimeMs;
}

// ----------------------------------- QnCompressedAudioData -----------------------------------------

void QnCompressedAudioData::assign(const QnCompressedAudioData* other)
{
    QnAbstractMediaData::assign(other);
    duration = other->duration;
}

QnCompressedAudioData* QnCompressedAudioData::clone() const
{
    QnCompressedAudioData* rez = new QnCompressedAudioData(data.getAlignment(), data.size());
    rez->assign(this);
    return rez;
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
