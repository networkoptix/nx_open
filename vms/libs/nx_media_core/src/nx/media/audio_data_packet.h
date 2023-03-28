// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/media/config.h>

#include "media_data_packet.h"

class NX_MEDIA_CORE_API QnCompressedAudioData: public QnAbstractMediaData
{
public:
    //QnCodecAudioFormat format;
    quint64 duration;

    QnCompressedAudioData(const CodecParametersConstPtr& ctx);
    quint64 getDurationMs() const;
    void assign(const QnCompressedAudioData* other);
};

typedef std::shared_ptr<QnCompressedAudioData> QnCompressedAudioDataPtr;
typedef std::shared_ptr<const QnCompressedAudioData> QnConstCompressedAudioDataPtr;


//!Stores video data buffer using \a nx::utils::ByteArray
class NX_MEDIA_CORE_API QnWritableCompressedAudioData: public QnCompressedAudioData
{
public:
    nx::utils::ByteArray m_data;

    /** Default FFMpeg alignment and padding are used. */
    QnWritableCompressedAudioData(size_t capacity = 0, CodecParametersConstPtr ctx = {});

    //!Implementation of QnAbstractMediaData::clone
    virtual QnWritableCompressedAudioData* clone() const override;
    //!Implementation of QnAbstractMediaData::data
    virtual const char* data() const override;
    //!Implementation of QnAbstractMediaData::dataSize
    virtual size_t dataSize() const override;

    virtual void setData(nx::utils::ByteArray&& buffer) override;

private:
    void assign(const QnWritableCompressedAudioData* other);
};

typedef std::shared_ptr<QnWritableCompressedAudioData> QnWritableCompressedAudioDataPtr;
typedef std::shared_ptr<const QnWritableCompressedAudioData> QnConstWritableCompressedAudioDataPtr;
