#ifndef abstract_audio_decoder_h_1532
#define abstract_audio_decoder_h_1532

#include "audio_struct.h"
#include "data/mediadata.h"

class CLAbstractAudioDecoder
{
public:
	explicit CLAbstractAudioDecoder(){}
	virtual ~CLAbstractAudioDecoder(){};

	virtual bool decode(CLAudioData& )=0;

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
	static CLAbstractAudioDecoder* createDecoder(CLCompressedAudioData* data);
};

#endif //abstract_audio_decoder_h_1532
