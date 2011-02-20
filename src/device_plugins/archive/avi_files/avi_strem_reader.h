#ifndef avi_stream_reader_h1901
#define avi_stream_reader_h1901

#include "..\abstract_archive_stream_reader.h"
#include "data/mediadata.h"



struct AVFormatContext;

class CLAVIStreamReader : public CLAbstractArchiveReader
{
public:
	CLAVIStreamReader(CLDevice* dev);
	virtual ~CLAVIStreamReader();

	virtual quint64 currTime() const;

protected:
	virtual CLAbstractMediaData* getNextData();
	virtual void channeljumpTo(quint64 mksec, int channel);

	bool getNextPacket();

	bool init();
	void destroy();

	void smart_sleep(quint64 mksec);

protected:
	qint64 m_currentTime;
	qint64 m_prev_time;

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

	int m_audioClock;
	int m_videoClock;
};

#endif //avi_stream_reader_h1901