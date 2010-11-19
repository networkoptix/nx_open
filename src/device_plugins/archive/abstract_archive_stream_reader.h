#ifndef abstract_archive_stream_reader_h1907
#define abstract_archive_stream_reader_h1907

#include "streamreader\cpull_stremreader.h"
#include "device\device_video_layout.h"

class CLAbstractArchiveReader : public CLClientPullStreamreader
{
protected:
	struct ArchiveFrameInfo
	{
		unsigned int size;
		QDateTime abs_time;
		unsigned int code;
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

	// returns len of archive in msec
	unsigned long len_msec() const;
	virtual void jumpTo(unsigned long msec, bool makeshot);
	

protected:
	virtual unsigned int nextChannel() const;
	virtual void jumpTo(unsigned long msec, int channel);

	int nextFrameIndex(int channel, bool keyframe, bool forwarddirection) const;
	int findBestIndex(int channel, unsigned long msec) const;

protected:
	QList<ArchiveFrameInfo> mMovie[CL_MAX_CHANNELS];
	unsigned int mCurrIndex[CL_MAX_CHANNELS];

	unsigned long m_len_msec;
	bool mForward;
	bool mSingleShot;
};



#endif //abstract_archive_stream_reader_h1907