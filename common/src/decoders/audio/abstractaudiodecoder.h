#ifndef abstract_audio_decoder_h_1532
#define abstract_audio_decoder_h_1532

#ifdef ENABLE_DATA_PROVIDERS

#include "audio_struct.h"
#include "core/datapacket/media_data_packet.h"
#include "core/datapacket/audio_data_packet.h"

// TODO mike: Rename class and files to QnAbstractAudioDecoder.

class CLAbstractAudioDecoder;

class CLAbstractAudioDecoder
{
public:
    explicit CLAbstractAudioDecoder(){}
    virtual ~CLAbstractAudioDecoder(){};

    virtual bool decode(QnCompressedAudioDataPtr& data, QnByteArray& result) = 0;

private:
    CLAbstractAudioDecoder(const CLAbstractAudioDecoder&) {}
    CLAbstractAudioDecoder& operator=(CLAbstractAudioDecoder&) { return *this; }

};

class CLAudioDecoderFactory
{
public:
    /**
      * Passing ffmpeg codec-id. In case of other decoder we'll change it to some decoder-independent type and add decoder framework id as parameter.
      */
    static CLAbstractAudioDecoder* createDecoder(QnCompressedAudioDataPtr data);
};

#endif // ENABLE_DATA_PROVIDERS

#endif //abstract_audio_decoder_h_1532
