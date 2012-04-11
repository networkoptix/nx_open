#ifndef audio_struct_h1530
#define audio_struct_h1530

#ifndef Q_OS_WIN
#include "utils/media/audioformat.h"
#else
#include <QAudioFormat>
#define QnAudioFormat QAudioFormat
#endif

class CLAlignedData;

// input to the decoder 
struct CLAudioData
{
	CodecID codec; 

	const unsigned char* inbuf; // pointer to compressed data
	int inbuf_len; // compressed data len

	CLAlignedData* outbuf; // pointer where decoder puts decompressed data;
	unsigned long outbuf_len;

	QnAudioFormat format;
};

#endif //audio_struct_h1530
