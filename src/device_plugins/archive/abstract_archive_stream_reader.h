#ifndef abstract_archive_stream_reader_h1907
#define abstract_archive_stream_reader_h1907

#include "streamreader\cpull_stremreader.h"
#include "device\device_video_layout.h"
#include "base\adaptivesleep.h"

class CLAbstractArchiveReader : public CLClientPullStreamreader
{
public:
	CLAbstractArchiveReader(CLDevice* dev);
	virtual ~CLAbstractArchiveReader();

	void setSingleShotMode(bool single);
	bool isSingleShotMode() const;

	void setdirection(bool forward);
	bool isForwardDirection() const;

	virtual quint64 currentTime() const = 0;

	/**
      * @return length of archive in mksec
      */
	quint64 lengthMksec() const;

	virtual void jumpTo(quint64 mksec, bool makeshot);

protected:
	virtual void channeljumpTo(quint64 mksec, int channel) = 0;

protected:
	quint64 m_lengthMksec;
	bool m_forward;
	bool m_singleShot;
	CLAdaptiveSleep m_adaptiveSleep;
	qint64 m_needToSleep;

	mutable QMutex m_cs;

	bool m_useTwice;
};

#endif //abstract_archive_stream_reader_h1907