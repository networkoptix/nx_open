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
	virtual ~CLAVIStreamReader();

	virtual unsigned long currTime() const;

protected:

	virtual CLAbstractMediaData* getNextData();
	virtual void channeljumpTo(unsigned long msec, int channel);


	bool getNextPacket();

	bool init();
	void destroy();

	void smart_sleep(int msec);

protected:

	unsigned long m_cuur_time;
	int m_prev_time;

	AVFormatContext* m_formatContext;

	int	m_videoStrmIndex;
	int	m_audioStrmIndex;

	AVPacket m_packet;

	CLCodecType m_videocodec_id;
	CLCodecType m_audiocodec_id;

	int m_freq;
	int m_channels;

	bool mFirstTime;


	volatile bool m_wakeup;

	bool m_bsleep;
	

};

#endif //avi_stream_reader_h1901