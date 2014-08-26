#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#ifdef ENABLE_DATA_PROVIDERS

extern "C"
{
#include "libavcodec/avcodec.h"
}

#include <QtCore/QVector>
#include <QtCore/QRect>

// TODO: #Elric implement this ifdef on utils/media/audioformat.h level.
#ifndef Q_OS_WIN
#   include <utils/media/audioformat.h> 
#else
#   include <QtMultimedia/QAudioFormat>
#   define QnAudioFormat QAudioFormat
#endif

#include <utils/common/byte_array.h>
#include <utils/media/sse_helper.h>
#include <utils/common/aligned_allocator.h>
#include <utils/memory/abstract_allocator.h>
#include <utils/memory/system_allocator.h>
#include <utils/math/math.h>

#include "abstract_data_packet.h"

class QIODevice;

struct AVCodecContext;

namespace nxcip {
    class Picture;
}

// TODO: #Elric #enum
enum MediaQuality { 
    MEDIA_Quality_High = 1,  // high quality
    MEDIA_Quality_Low = 2,   // low quality
    // At current version MEDIA_Quality_ForceHigh is very similar to MEDIA_Quality_High. It used for export to 'avi' or 'mkv'. 
    // This mode do not tries first short LQ chunk if LQ chunk has slightly better position
    MEDIA_Quality_ForceHigh,
    MEDIA_Quality_Auto,
    MEDIA_Quality_None
};

class QnMediaContext: public QnAbstractMediaContext {
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
    enum MediaFlag {
        MediaFlags_None                 = 0x00000,
        MediaFlags_AVKey                = 0x00001,  /**< KeyFrame, must be equal to AV_PKT_FLAG_KEY from avcodec.h, checked via static_assert below. */
        MediaFlags_AfterEOF             = 0x00002,
        MediaFlags_BOF                  = 0x00004,
        MediaFlags_LIVE                 = 0x00008,
        MediaFlags_Ignore               = 0x00010,
                     
        MediaFlags_ReverseReordered     = 0x00020,
        MediaFlags_ReverseBlockStart    = 0x00040,
        MediaFlags_Reverse              = 0x00080,

        MediaFlags_LowQuality           = 0x00100,
        MediaFlags_StillImage           = 0x00200,

        MediaFlags_NewServer            = 0x00400, /**< switch archive to a new server */
        MediaFlags_DecodeTwice          = 0x00800,
        MediaFlags_FCZ                  = 0x01000, /**< fast channel zapping flag */
        MediaFlags_AfterDrop            = 0x02000, /**< some data was dropped before current data */

        MediaFlags_HWDecodingUsed       = 0x04000, /**< hardware decoding is used */
        MediaFlags_PlayUnsync           = 0x08000, /**< ignore syncplay mode */
        MediaFlags_Skip                 = 0x10000, /**< ignore packet at all */
    };
    Q_DECLARE_FLAGS(MediaFlags, MediaFlag)

    // TODO: #Elric #enum
    enum DataType {
        VIDEO, 
        AUDIO, 
        CONTAINER, 
        META_V1, 
        EMPTY_DATA
    };

    //QnAbstractMediaData(unsigned int alignment, unsigned int capacity): 
    QnAbstractMediaData( DataType _dataType = EMPTY_DATA );
    virtual ~QnAbstractMediaData();

    bool isLQ() const { return flags & MediaFlags_LowQuality; }
    bool isLive() const { return flags & MediaFlags_LIVE; }

    virtual QnAbstractMediaData* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const = 0;
    virtual const char* data() const = 0;
    virtual size_t dataSize() const = 0;

    DataType dataType;
    CodecID compressionType;
    MediaFlags flags;
    quint32 channelNumber;     // video or audio channel number; some devices might have more than one sensor
    QnMediaContextPtr context;
    int opaque;
protected:
    void assign(const QnAbstractMediaData* other);
private:
    //QnAbstractMediaData(): data(0U, 1) {};
};
typedef QSharedPointer<QnAbstractMediaData> QnAbstractMediaDataPtr;
typedef QSharedPointer<const QnAbstractMediaData> QnConstAbstractMediaDataPtr;

Q_STATIC_ASSERT(AV_PKT_FLAG_KEY == QnAbstractMediaData::MediaFlags_AVKey);
Q_DECLARE_OPERATORS_FOR_FLAGS(QnAbstractMediaData::MediaFlags)


struct QnEmptyMediaData : public QnAbstractMediaData
{
    QnEmptyMediaData(): m_data(16,0)
    {
        dataType = EMPTY_DATA;
    }

    QnEmptyMediaData( QnAbstractAllocator* allocator )
    :
        m_data(allocator, 16, 0)
    {
        dataType = EMPTY_DATA;
    }

    virtual QnEmptyMediaData* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const override;
    virtual const char* data() const override { return m_data.data(); }
    virtual size_t dataSize() const override { return m_data.size(); }

    QnByteArray m_data;
};
typedef QSharedPointer<QnEmptyMediaData> QnEmptyMediaDataPtr;

struct QnMetaDataV1;
typedef QSharedPointer<QnMetaDataV1> QnMetaDataV1Ptr;
Q_DECLARE_METATYPE(QnMetaDataV1Ptr);
typedef QSharedPointer<const QnMetaDataV1> QnConstMetaDataV1Ptr;
Q_DECLARE_METATYPE(QnConstMetaDataV1Ptr);


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
        startTimeMs = qFromBigEndian(startTimeMs);
        durationMs = qFromBigEndian(durationMs);
    }

    qint64 endTimeMs() const  { return startTimeMs + durationMs; }

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

typedef std::vector<QnMetaDataV1Light, QnAlignedAllocator<QnMetaDataV1Light> > QnMetaDataLightVector;


struct QnMetaDataV1 : public QnAbstractMediaData
{
    QnMetaDataV1(int initialValue = 0);
    QnMetaDataV1(
        QnAbstractAllocator* allocator,
        int initialValue = 0);

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
    void addMotion(QnConstMetaDataV1Ptr data);

    // remove part of motion info by motion mask
    void removeMotion(const simd128i* data, int startIndex = 0, int endIndex = MD_WIDTH*MD_HEIGHT/128 - 1);

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

    //!Copies \a motionPicture data
    void assign( const nxcip::Picture& motionPicture, qint64 timestamp, qint64 duration );


    static void createMask(const QRegion& region,  char* mask, int* maskStart = 0, int* maskEnd = 0);

    virtual QnMetaDataV1* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const override;
    virtual const char* data() const override { return m_data.data(); }
    char* data() { return m_data.data(); }
    virtual size_t dataSize() const override { return m_data.size(); }

    //void deserialize(QIODevice* ioDevice);
    void serialize(QIODevice* ioDevice) const;

    static bool matchImage(const simd128i* data, const simd128i* mask, int maskStart = 0, int maskEnd = MD_WIDTH * MD_HEIGHT / 128 - 1);


    quint8 m_input;
    qint64 m_duration;
    QnByteArray m_data;

protected:
    void assign(const QnMetaDataV1* other);

private:
    qint64 m_firstTimestamp;
};

#endif // ENABLE_DATA_PROVIDERS

#endif //abstract_media_data_h_112
