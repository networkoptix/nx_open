#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#include <QVector>
#include <QRect>
#include "libavcodec/avcodec.h"
#include "abstract_data_packet.h"
#include "utils/common/byte_array.h"
#include "utils/media/sse_helper.h"

#ifndef Q_OS_WIN
#   include "utils/media/audioformat.h"
#else
#   include <QAudioFormat>
#   define QnAudioFormat QAudioFormat
#endif
#include "utils/common/math.h"
#include "utils/network/socket.h"
#include "utils/common/util.h"

struct AVCodecContext;

enum MediaQuality { MEDIA_Quality_High, MEDIA_Quality_Low, MEDIA_Quality_AlwaysHigh, MEDIA_Quality_None};

class QnMediaContext {
public:

    QnMediaContext(AVCodecContext* ctx);
    QnMediaContext(CodecID codecId);
    QnMediaContext(const quint8* payload, int dataSize);
    QnMediaContext(const QByteArray& payload);
    ~QnMediaContext();
    AVCodecContext* ctx() const;
    QString codecName() const;

    bool equalTo(QnMediaContext* other) const;

private:
    AVCodecContext* m_ctx;
};

typedef QSharedPointer<QnMediaContext> QnMediaContextPtr;

struct QnAbstractMediaData : public QnAbstractDataPacket
{
    enum MediaFlags {
        MediaFlags_None = 0, 
        //MediaFlags_Key = 1,
        MediaFlags_AfterEOF = 2,
        MediaFlags_BOF = 4,
        MediaFlags_LIVE = 8,
        MediaFlags_Ignore = 16,
                     
        MediaFlags_ReverseReordered = 32,
        MediaFlags_ReverseBlockStart = 64,
        MediaFlags_Reverse = 128,

        MediaFlags_LowQuality = 256,
        MediaFlags_StillImage = 512,

        MediaFlags_NewServer = 1024, // swith archive to a new media server
        MediaFlags_DecodeTwice = 2048,
        MediaFlags_FCZ = 4096 // fast channel zapping flag
    };

    enum DataType {
        VIDEO, 
        AUDIO, 
        CONTAINER, 
        META_V1, 
        EMPTY_DATA
    };

    QnAbstractMediaData(unsigned int alignment, unsigned int capacity): 
        data(alignment, capacity),
        dataType(EMPTY_DATA),
        compressionType(CODEC_ID_NONE),
        flags(MediaFlags_None),
        channelNumber(0),
        context(0),
        opaque(0)
    {
    }

    virtual QnAbstractMediaData* clone();

    //!Create media packet using existing data \a data of size \a dataSize. This buffer will not be deleted!
    QnAbstractMediaData(char* data, unsigned int dataSize): 
        data(data, dataSize),
        dataType(EMPTY_DATA),
        compressionType(CODEC_ID_NONE),
        flags(MediaFlags_None),
        channelNumber(0),
        context(0),
        opaque(0)
    {
    }

    virtual ~QnAbstractMediaData()
    {
    }

    QnByteArray data;
    DataType dataType;
    CodecID compressionType;
    unsigned flags;
    quint32 channelNumber;     // video or audio channel number; some devices might have more that one sensor.
    QnMediaContextPtr context;
    int opaque;
protected:
    void assign(QnAbstractMediaData* other);
private:
    QnAbstractMediaData(): data(0U, 1) {};
};
typedef QSharedPointer<QnAbstractMediaData> QnAbstractMediaDataPtr;


struct QnEmptyMediaData : public QnAbstractMediaData
{
    QnEmptyMediaData(): QnAbstractMediaData(16,0)
    {
        dataType = EMPTY_DATA;
    }
};
typedef QSharedPointer<QnEmptyMediaData> QnEmptyMediaDataPtr;

struct QnMetaDataV1;
typedef QSharedPointer<QnMetaDataV1> QnMetaDataV1Ptr;


struct QnCompressedVideoData : public QnAbstractMediaData
{
    QnCompressedVideoData(unsigned int alignment, unsigned int capacity, QnMediaContextPtr ctx = QnMediaContextPtr(0))
        : QnAbstractMediaData(alignment, qMin(capacity, (unsigned int)10 * 1024 * 1024))
    {
        dataType = VIDEO;
        //useTwice = false;
        context = ctx;
        //ignore = false;
        flags = 0;
        width = height = -1;
        pts = AV_NOPTS_VALUE;
    }

    virtual QnCompressedVideoData* clone() override;


    int width;
    int height;
    //bool keyFrame;
    //int flags;
    //bool ignore;
    QnMetaDataV1Ptr motion;
    qint64 pts;
protected:
    void assign(QnCompressedVideoData* other);
};

typedef QSharedPointer<QnCompressedVideoData> QnCompressedVideoDataPtr;

enum {MD_WIDTH = 44, MD_HEIGHT = 32};

/** 
* This structure used for serialized QnMetaDataV1
* Timestamp and duration specified in milliseconds
* structure can be directly mapped to deserialized memory buffer to represent MetaData
*/
#pragma pack(push, 1)
struct QnMetaDataV1Light
{

    /** 
    * Structure MUST be prepared before use by calling doMarshalling method
    */
    void doMarshalling()
    {
        startTimeMs = ntohll(startTimeMs);
        durationMs = ntohl(durationMs);
    }

    qint64 endTimeMs()   const  { return startTimeMs + durationMs; }

    quint64 startTimeMs;
    quint32 durationMs;
    quint8 channel;
    quint8 input;
    quint16 reserved;
    quint8 data[MD_WIDTH*MD_HEIGHT/8];
};
#pragma pack(pop)
bool operator< (const QnMetaDataV1Light& data, const quint64 timeMs);
bool operator< (const quint64 timeMs, const QnMetaDataV1Light& data);

typedef std::vector<QnMetaDataV1Light, AlignmentAllocator<QnMetaDataV1Light> > QnMetaDataLightVector;

struct QnMetaDataV1 : public QnAbstractMediaData
{
    QnMetaDataV1(int initialValue = 0);

    static QnMetaDataV1Ptr fromLightData(const QnMetaDataV1Light& lightData);

    /** 
    * Merge existing motion image with new motion image. Matrix is allowed col to col 
    * 0   1
    * |   |
    * \/  \/
    * |   | 
    * \/  \/
    * |   |
    * \/  \/
    */
    void addMotion(const quint8* data, qint64 timestamp);
    void addMotion(QnMetaDataV1Ptr data);

    // remove part of motion info by motion mask
    void removeMotion(const __m128i* data, int startIndex = 0, int endIndex = MD_WIDTH*MD_HEIGHT/128 - 1);

    // ti check if we've got motion at 
    static bool isMotionAt(int x, int y, char* mask);

    // ti check if we've got motion at 
    bool isMotionAt(int x, int y) const;

    void setMotionAt(int x, int y);

    bool mapMotion(const QRect& imageRect, const QRect& mRect);

    bool isInput(int index) const
    {
        //unsigned char b = *((unsigned char*)data.data() + MD_WIDTH * MD_HIGHT/8);
        //return (b>>index) & 1 ;
        return (m_input >> index) & 1;
    }

    bool containTime(const qint64 timeUsec) const;

    /** returns true if no motion detected */
    bool isEmpty() const;


    static void createMask(const QRegion& region,  char* mask, int* maskStart = 0, int* maskEnd = 0);

    virtual QnMetaDataV1* clone() override;

    //void deserialize(QIODevice* ioDevice);
    void serialize(QIODevice* ioDevice);

    static bool mathImage(const __m128i* data, const __m128i* mask, int maskStart = 0, int maskEnd = MD_WIDTH * MD_HEIGHT / 128 - 1);


    quint8 m_input;
    qint64 m_duration;
protected:
    void assign(QnMetaDataV1* other);
private:
    qint64 m_firstTimestamp;
};


class QnCodecAudioFormat: public QnAudioFormat
{
public:
    QnCodecAudioFormat():
        QnAudioFormat(),
        bitrate(0),
        channel_layout(0),
        block_align(0),
        m_bitsPerSample(0)
    {}

    QnCodecAudioFormat(QnMediaContextPtr ctx)
    {
        fromAvStream(ctx);
    }

    QnCodecAudioFormat& fromAvStream(QnMediaContextPtr ctx)
    {
        fromAvStream(ctx->ctx());
        return *this;
    }

    void fromAvStream(AVCodecContext* c)
    {
        if (c->sample_rate)
            setFrequency(c->sample_rate);

        if (c->channels) 
            setChannels(c->channels);

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

    QVector<quint8> extraData; // codec extra data
    int bitrate;
    int channel_layout;
    int block_align;
    int m_bitsPerSample;
};


struct QnCompressedAudioData : public QnAbstractMediaData
{
    QnCompressedAudioData (unsigned int alignment, unsigned int capacity, QnMediaContextPtr ctx = QnMediaContextPtr(0))
        : QnAbstractMediaData(alignment, capacity)
    {
        dataType = AUDIO;
        duration = 0;
        context = ctx;
    }

    virtual QnCompressedAudioData* clone() override;

    //QnCodecAudioFormat format;
    quint64 duration;
private:
    void assign(QnCompressedAudioData* other);
};
typedef QSharedPointer<QnCompressedAudioData> QnCompressedAudioDataPtr;

//typedef CLThreadQueue<QnAbstractDataPacketPtr> CLDataQueue;

#endif //abstract_media_data_h_112
