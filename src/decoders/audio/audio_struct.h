#ifndef audio_struct_h1530
#define audio_struct_h1530

#include "data\mediadata.h"

class CLAudioDecoderOutput
{

};


// input to the decoder 
struct CLAudioData
{

	CLCodecType codec; 

	//out frame info;
	//client needs only define ColorSpace out_type; decoder will setup ather variables
	CLAudioDecoderOutput uncompressed_audio_data; 

	const unsigned char* inbuf; // pointer to compressed data
	int buff_len; // compressed data len

};

#endif //audio_struct_h1530