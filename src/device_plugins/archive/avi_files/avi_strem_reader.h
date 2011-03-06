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

	virtual quint64 currentTime() const;

protected:
	virtual CLAbstractMediaData* getNextData();
	virtual void channeljumpTo(quint64 mksec, int channel);

	bool getNextPacket();

	bool init();
	void destroy();

	void smartSleep(quint64 mksec);

protected:
	qint64 m_currentTime;
	qint64 m_previousTime;

	AVFormatContext* m_formatContext;

	int	m_videoStreamIndex;
	int	m_audioStreamIndex;

	AVPacket m_packet;

	CLCodecType m_videoCodecId;
	CLCodecType m_audioCodecId;

	int m_freq;
	int m_channels;

	bool mFirstTime;

	volatile bool m_wakeup;

	bool m_bsleep;
};

#endif //avi_stream_reader_h1901