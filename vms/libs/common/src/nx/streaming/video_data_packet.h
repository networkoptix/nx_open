#pragma once

#include <memory>

#include <utils/common/byte_array.h>

#include <nx/streaming/config.h>
#include <nx/streaming/media_data_packet.h>


constexpr auto MAX_ALLOWED_FRAME_SIZE = 1024*1024*10;

//!Contains video data specific fields. Video data buffer stored in a child class
class QnCompressedVideoData
:
    public QnAbstractMediaData
{
public:
    int width;
    int height;
    //bool keyFrame;
    //int flags;
    //bool ignore;
    FrameMetadata metadata;
    qint64 pts;

    QnCompressedVideoData( QnConstMediaContextPtr ctx = QnConstMediaContextPtr(nullptr) );

    //!Implementation of QnAbstractMediaData::clone
    /*!
        Does nothing. Overridden method MUST return \a QnWritableCompressedVideoData
    */
    virtual QnCompressedVideoData* clone() const override = 0;

    void assign(const QnCompressedVideoData* other);
};

typedef std::shared_ptr<QnCompressedVideoData> QnCompressedVideoDataPtr;
typedef std::shared_ptr<const QnCompressedVideoData> QnConstCompressedVideoDataPtr;


//!Stores video data buffer using \a QnByteArray
class QnWritableCompressedVideoData: public QnCompressedVideoData
{
public:
    QnByteArray m_data;

    /** Default FFMpeg alignment and padding are used. */
    QnWritableCompressedVideoData(size_t capacity = 0, QnConstMediaContextPtr ctx = {});

    QnWritableCompressedVideoData(
        size_t alignment,
        size_t capacity,
        size_t padding,
        QnConstMediaContextPtr ctx = {});

    //!Implementation of QnAbstractMediaData::clone
    virtual QnWritableCompressedVideoData* clone() const override;
    //!Implementation of QnAbstractMediaData::data
    virtual const char* data() const override;
    //!Implementation of QnAbstractMediaData::dataSize
    virtual size_t dataSize() const override;

private:
    void assign(const QnWritableCompressedVideoData* other);
};

typedef std::shared_ptr<QnWritableCompressedVideoData> QnWritableCompressedVideoDataPtr;
typedef std::shared_ptr<const QnWritableCompressedVideoData> QnConstWritableCompressedVideoDataPtr;
