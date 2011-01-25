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

	virtual unsigned long currTime() const = 0;

	// returns len of archive in msec
	unsigned long len_msec() const;
	virtual void jumpTo(unsigned long msec, bool makeshot);
	

protected:

	virtual void channeljumpTo(unsigned long msec, int channel) = 0;

protected:

	unsigned long m_len_msec;
	bool mForward;
	bool mSingleShot;
	CLAdaptiveSleep mAdaptiveSleep;
	int m_need_tosleep;

	mutable QMutex m_cs;

	bool m_use_twice;

};



#endif //abstract_archive_stream_reader_h1907