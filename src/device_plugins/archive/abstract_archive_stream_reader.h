#ifndef abstract_archive_stream_reader_h1907
#define abstract_archive_stream_reader_h1907

#include "streamreader\cpull_stremreader.h"
#include "device\device_video_layout.h"
#include "base\adaptivesleep.h"

class CLAbstractArchiveReader : public CLClientPullStreamreader
{
public:
	CLAbstractArchiveReader(CLDevice* dev );
	virtual ~CLAbstractArchiveReader();

	void setSingleShotMode(bool single);
	bool isSingleShotMode() const;

	void setdirection(bool forward);
	bool isForwardDirection() const;

	virtual quint64 currTime() const = 0;

	// returns len of archive in mksec
	quint64 len_mks() const;
	virtual void jumpTo(quint64 mksec, bool makeshot);
	

protected:

	virtual void channeljumpTo(quint64 mksec, int channel) = 0;

protected:

	quint64 m_len_mksec;
	bool mForward;
	bool mSingleShot;
	CLAdaptiveSleep mAdaptiveSleep;
	qint64 m_need_tosleep;

	mutable QMutex m_cs;

	bool m_use_twice;

};



#endif //abstract_archive_stream_reader_h1907