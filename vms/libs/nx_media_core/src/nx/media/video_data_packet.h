// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/media/config.h>
#include <nx/media/media_data_packet.h>
#include <nx/media/meta_data_packet.h>
#include <nx/utils/byte_array.h>

constexpr auto MAX_ALLOWED_FRAME_SIZE = 1024*1024*10;

//!Contains video data specific fields. Video data buffer stored in a child class
class NX_MEDIA_CORE_API QnCompressedVideoData: public QnAbstractMediaData
{
public:
    int width;
    int height;
    //bool keyFrame;
    //int flags;
    //bool ignore;
    FrameMetadata metadata;
    qint64 pts;

    QnCompressedVideoData( CodecParametersConstPtr ctx = CodecParametersConstPtr(nullptr) );

    //!Implementation of QnAbstractMediaData::clone
    /*!
        Does nothing. Overridden method MUST return \a QnWritableCompressedVideoData
    */
    virtual QnCompressedVideoData* clone() const override = 0;

    void assign(const QnCompressedVideoData* other);
};

typedef std::shared_ptr<QnCompressedVideoData> QnCompressedVideoDataPtr;
typedef std::shared_ptr<const QnCompressedVideoData> QnConstCompressedVideoDataPtr;


//!Stores video data buffer using \a nx::utils::ByteArray
class NX_MEDIA_CORE_API QnWritableCompressedVideoData: public QnCompressedVideoData
{
public:
    nx::utils::ByteArray m_data;

    /** Default FFMpeg alignment and padding are used. */
    QnWritableCompressedVideoData(size_t capacity = 0, CodecParametersConstPtr ctx = {});

    //!Implementation of QnAbstractMediaData::clone
    virtual QnWritableCompressedVideoData* clone() const override;
    //!Implementation of QnAbstractMediaData::data
    virtual const char* data() const override;
    //!Implementation of QnAbstractMediaData::dataSize
    virtual size_t dataSize() const override;

    virtual void setData(nx::utils::ByteArray&& buffer) override;

private:
    void assign(const QnWritableCompressedVideoData* other);
};

typedef std::shared_ptr<QnWritableCompressedVideoData> QnWritableCompressedVideoDataPtr;
typedef std::shared_ptr<const QnWritableCompressedVideoData> QnConstWritableCompressedVideoDataPtr;
