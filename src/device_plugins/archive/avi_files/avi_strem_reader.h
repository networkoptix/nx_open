#ifndef avi_stream_reader_h1901
#define avi_stream_reader_h1901

#include "../abstract_archive_stream_reader.h"
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

	bool init();
	void destroy();

	void smartSleep(quint64 mksec);

protected:
	qint64 m_currentTime;
	qint64 m_previousTime;

	AVFormatContext* m_formatContext;

	int	m_videoStreamIndex;
	int	m_audioStreamIndex;

	CLCodecType m_videoCodecId;
	CLCodecType m_audioCodecId;

	int m_freq;
	int m_channels;

	bool mFirstTime;

	volatile bool m_wakeup;

	bool m_bsleep;

private:
    AVPacket m_packets[2];
    int m_currentPacketIndex;
    bool m_haveSavedPacket;

private:
    /**
      * Read next packet from file
      *
      * @param packet packet to store data to
      *
      * @return true if successful
      */
	bool getNextPacket(AVPacket& packet);

    /**
      * Read next video packet from file ignoring audio packets.
      * Store it to packet referenced by nextPacket()
      * 
      * @return true if successful
      */
    bool getNextVideoPacket();

    /**
      * Create CLCompressedVideoData object and fill it from StreamReader state, packet and codec context
      *
      * @return created object
      */
    CLCompressedVideoData* getVideoData(const AVPacket& packet, AVCodecContext* codecContext);

    /**
      * Create CLCompressedAudioData object and fill it from StreamReader state, packet and AVStream
      *
      * @return created object
      */
    CLCompressedAudioData* getAudioData(const AVPacket& packet, AVStream* stream);

    /**
      * Replace current and next packet references.
      */
    void switchPacket();

    AVPacket& currentPacket();
    AVPacket& nextPacket();
};

#endif //avi_stream_reader_h1901
