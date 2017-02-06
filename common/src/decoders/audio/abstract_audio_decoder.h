#ifndef ABSTRACT_AUDIO_DECODER_H
#define ABSTRACT_AUDIO_DECODER_H

#ifdef ENABLE_DATA_PROVIDERS

#include "audio_struct.h"
#include "nx/streaming/media_data_packet.h"
#include "nx/streaming/audio_data_packet.h"

class QnAbstractAudioDecoder
{
public:
    explicit QnAbstractAudioDecoder(){}
    virtual ~QnAbstractAudioDecoder(){};

    virtual bool decode(QnCompressedAudioDataPtr& data, QnByteArray& result) = 0;

private:
    QnAbstractAudioDecoder(const QnAbstractAudioDecoder&) {}
    QnAbstractAudioDecoder& operator=(QnAbstractAudioDecoder&) { return *this; }

};

class QnAudioDecoderFactory
{
public:
    /**
      * Passing ffmpeg codec-id. In case of other decoder we'll change it to some decoder-independent type and add decoder framework id as parameter.
      */
    static QnAbstractAudioDecoder* createDecoder(QnCompressedAudioDataPtr data);
};

#endif // ENABLE_DATA_PROVIDERS

#endif // ABSTRACT_AUDIO_DECODER_H
