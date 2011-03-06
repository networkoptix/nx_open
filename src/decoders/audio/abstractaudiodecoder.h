#ifndef abstract_audio_decoder_h_1532
#define abstract_audio_decoder_h_1532

#include "audio_struct.h"

class CLAbstractAudioDecoder
{
public:
	explicit CLAbstractAudioDecoder(){}
	virtual ~CLAbstractAudioDecoder(){};

	virtual bool decode(CLAudioData& )=0;

private:
	CLAbstractAudioDecoder(const CLAbstractAudioDecoder&){};
	CLAbstractAudioDecoder& operator=(CLAbstractAudioDecoder&){};

};

class CLAudioDecoderFactory
{
public:
	static CLAbstractAudioDecoder* createDecoder(CLCodecType codec, void* codecContext);
};

#endif //abstract_audio_decoder_h_1532