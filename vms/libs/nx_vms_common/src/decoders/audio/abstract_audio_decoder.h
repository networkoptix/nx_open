// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/audio_data_packet.h>
#include <nx/media/media_data_packet.h>

class QnAbstractAudioDecoder
{
public:
    explicit QnAbstractAudioDecoder(){}
    virtual ~QnAbstractAudioDecoder(){};

    virtual bool decode(QnCompressedAudioDataPtr& data, nx::utils::ByteArray& result) = 0;

private:
    QnAbstractAudioDecoder(const QnAbstractAudioDecoder&) {}
    QnAbstractAudioDecoder& operator=(QnAbstractAudioDecoder&) { return *this; }

};

class NX_VMS_COMMON_API QnAudioDecoderFactory
{
public:
    /**
      * Passing ffmpeg codec-id. In case of other decoder we'll change it to some decoder-independent type and add decoder framework id as parameter.
      */
    static QnAbstractAudioDecoder* createDecoder(QnCompressedAudioDataPtr data);
};
