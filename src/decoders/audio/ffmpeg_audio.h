#ifndef cl_ffmpeg_h
#define cl_ffmpeg_h

#include "abstractaudiodecoder.h"

// client of this class is responsible for encoded data buffer meet ffmpeg restrictions
// ( see comment to decode functions for details ).

class CLFFmpegAudioDecoder : public CLAbstractAudioDecoder
{
public:
	CLFFmpegAudioDecoder(CodecID codec, AVCodecContext* codecContext);
	bool decode(CLAudioData& data);
	~CLFFmpegAudioDecoder();

private:

	AVCodec *codec;
	AVCodecContext *c;

	static bool m_first_instance;
	CodecID m_codec;

	//===================
};

#endif //cl_ffmpeg_h

