#ifndef avi_stream_reader_h1901
#define avi_stream_reader_h1901

#include "..\abstract_archive_stream_reader.h"
#include "libavcodec/avcodec.h"
#include "data/mediadata.h"



struct AVFormatContext;

class CLAVIStreamReader : public CLAbstractArchiveReader
{
public:
	CLAVIStreamReader(CLDevice* dev );
	~CLAVIStreamReader();

	virtual unsigned long currTime() const;

protected:

	virtual CLAbstractMediaData* getNextData();
	virtual void channeljumpTo(unsigned long msec, int channel);


	bool getNextVideoPacket();

	bool init();
	void destroy();

protected:

	unsigned long cuur_time;

	AVFormatContext* m_formatContext;
	int	m_videoStrmIndex;
	AVPacket m_packet;
	CLVideoCodecType m_codec_id;
	
	

};

#endif //avi_stream_reader_h1901