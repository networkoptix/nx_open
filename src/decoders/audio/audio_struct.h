#ifndef audio_struct_h1530
#define audio_struct_h1530

#include "data\mediadata.h"


// input to the decoder 
struct CLAudioData
{
	CLCodecType codec; 

	const unsigned char* inbuf; // pointer to compressed data
	int inbuf_len; // compressed data len


	unsigned char* outbuf; // pointer where decoder puts decompressed data; user is responsable to provide big enought outbuf
	int outbuf_len;

	QAudioFormat format;
};

#endif //audio_struct_h1530