#ifndef archive_stream_reader_h1145
#define archive_stream_reader_h1145

#include "../abstract_archive_stream_reader.h"

class CLArchiveStreamReader  : public CLAbstractArchiveReader 
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
		quint64 time; // time in mks from beginning 
	};

public:
	CLArchiveStreamReader(CLDevice* dev);
	~CLArchiveStreamReader();

	quint64 currentTime() const;
	virtual void resume();

    bool setRecordedDataDst(const QString& dst);
protected:

	virtual void channeljumpTo(quint64 mksec, int channel);
	virtual CLAbstractMediaData* getNextData();

	// i do not want to do it in constructor to unload gui thread 
	// let reader thread do the work
	// this function will be called with first getNextData
	void init_data();

	void parse_channel_data(int channel, int data_version, char* data, unsigned int len);

	bool reachedTheEnd() const;
	int nextFrameIndex(bool after_jump, int channel, int curr_index, bool keyframe, bool forwarddirection) const;
	int findBestIndex(int channel, quint64 mksec) const;
	int slowest_channel() const;

    void setSkipFramesToTime(quint64 skipFramesToTime);

    bool isAllChannelsSkippedToTime() const;

protected:
	bool m_firsttime;

	QList<ArchiveFrameInfo> mMovie[CL_MAX_CHANNELS];
	unsigned int mCurrIndex[CL_MAX_CHANNELS];
	bool mFinished[CL_MAX_CHANNELS];

	QFile m_data_file[CL_MAX_CHANNELS];
	QDataStream m_data_stream[CL_MAX_CHANNELS];


    bool m_skippedToTime[CL_MAX_CHANNELS];
    QString mRecordedDataDst;
};

#endif //archive_stream_reader_h1145

