#ifndef abstract_archive_stream_reader_h1907
#define abstract_archive_stream_reader_h1907

#include "streamreader\cpull_stremreader.h"
#include "device\device_video_layout.h"
#include "base\adaptivesleep.h"

class CLAbstractArchiveReader : public CLClientPullStreamreader
{
protected:
	struct ArchiveFrameInfo
	{
		unsigned int size;
		quint64 abs_time;
		unsigned int codec;
		bool keyFrame;

		// shift inside file ( may be in bytes or some over units depends on implementation of CLClientPullStreamreader
		unsigned int shift;
		unsigned long time_ms; // time in ms from beginning 
	};
public:
	CLAbstractArchiveReader(CLDevice* dev );
	~CLAbstractArchiveReader();

	void setSingleShotMode(bool single);
	bool isSingleShotMode() const;

	void setdirection(bool forward);
	bool isForwardDirection() const;

	unsigned long currTime() const;

	// returns len of archive in msec
	unsigned long len_msec() const;
	virtual void jumpTo(unsigned long msec, bool makeshot);
	

protected:

	virtual void jumpTo(unsigned long msec, int channel);

	// if after_jump means we do not need to increase index 
	int nextFrameIndex(bool after_jump, int channel, int curr_index, bool keyframe, bool forwarddirection) const;


	int findBestIndex(int channel, unsigned long msec) const;

	bool reachedTheEnd() const;
	

	int slowest_channel() const;

protected:
	QList<ArchiveFrameInfo> mMovie[CL_MAX_CHANNELS];
	unsigned int mCurrIndex[CL_MAX_CHANNELS];

	unsigned long m_len_msec;
	bool mForward;
	bool mSingleShot;
	bool mFinished[CL_MAX_CHANNELS];
	CLAdaptiveSleep mAdaptiveSleep;
	int m_need_tosleep;

};



#endif //abstract_archive_stream_reader_h1907