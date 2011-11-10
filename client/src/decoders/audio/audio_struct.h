#ifndef audio_struct_h1530
#define audio_struct_h1530



class CLAlignedData;

// input to the decoder 
struct CLAudioData
{
	CodecID codec; 

	const unsigned char* inbuf; // pointer to compressed data
	int inbuf_len; // compressed data len

	CLAlignedData* outbuf; // pointer where decoder puts decompressed data;
	unsigned long outbuf_len;

	QAudioFormat format;
};

#endif //audio_struct_h1530
