/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#ifndef AUDIO_DATA_PACKET_H
#define AUDIO_DATA_PACKET_H

#include <memory>

#include <nx/streaming/config.h>
#include "media_data_packet.h"

class QnCodecAudioFormat
:
    public QnAudioFormat
{
public:
    QnCodecAudioFormat();
    QnCodecAudioFormat(const QnConstMediaContextPtr& c);

    QVector<quint8> extraData; //!< Codec extra data.
    int bitrate;
    int channel_layout;
    int block_align;
    int m_bitsPerSample;

private:
    //int m_frequency;
};


class QnCompressedAudioData
:
    public QnAbstractMediaData
{
public:
    //QnCodecAudioFormat format;
    quint64 duration;

    QnCompressedAudioData(const QnConstMediaContextPtr& ctx);
    quint64 getDurationMs() const;
    void assign(const QnCompressedAudioData* other);
};

typedef std::shared_ptr<QnCompressedAudioData> QnCompressedAudioDataPtr;
typedef std::shared_ptr<const QnCompressedAudioData> QnConstCompressedAudioDataPtr;


//!Stores video data buffer using \a QnByteArray
class QnWritableCompressedAudioData
:
    public QnCompressedAudioData
{
public:
    QnByteArray m_data;

    QnWritableCompressedAudioData(
        unsigned int alignment = CL_MEDIA_ALIGNMENT,
        unsigned int capacity = 0,
        QnConstMediaContextPtr ctx = QnConstMediaContextPtr(nullptr) );
    QnWritableCompressedAudioData(
        QnAbstractAllocator* allocator,
        unsigned int alignment = CL_MEDIA_ALIGNMENT,
        unsigned int capacity = 0,
        QnConstMediaContextPtr ctx = QnConstMediaContextPtr(nullptr) );

    //!Implementation of QnAbstractMediaData::clone
    virtual QnWritableCompressedAudioData* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const override;
    //!Implementation of QnAbstractMediaData::data
    virtual const char* data() const override;
    //!Implementation of QnAbstractMediaData::dataSize
    virtual size_t dataSize() const override;

private:
    void assign(const QnWritableCompressedAudioData* other);
};

typedef std::shared_ptr<QnWritableCompressedAudioData> QnWritableCompressedAudioDataPtr;
typedef std::shared_ptr<const QnWritableCompressedAudioData> QnConstWritableCompressedAudioDataPtr;

#endif  //AUDIO_DATA_PACKET_H
