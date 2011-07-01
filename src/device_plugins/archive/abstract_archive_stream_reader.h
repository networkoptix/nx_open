#ifndef abstract_archive_stream_reader_h1907
#define abstract_archive_stream_reader_h1907

#include "streamreader/cpull_stremreader.h"
#include "device/device_video_layout.h"
#include "base/adaptivesleep.h"

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
	bool isSkippingFrames() const;

	/**
      * @return length of archive in mksec
      */
	quint64 lengthMksec() const;
    quint64 startMksec() const;

	virtual void jumpTo(quint64 mksec, bool makeshot);
	void jumpToPreviousFrame(quint64 mksec, bool makeshot);

    virtual void previousFrame(quint64 /*mksec*/) {}
    virtual void nextFrame() {}


    // gives a list of audio tracks 
    virtual QStringList getAudioTracksInfo() const;

    //gets current audio channel ( if we have the only channel - returns 0 )
    virtual unsigned int getCurrentAudioChannel() const;

    // sets certain track 
    virtual bool setAudioChannel(unsigned int num);

protected:
	virtual void channeljumpTo(quint64 mksec, int channel) = 0;
    quint64 skipFramesToTime() const;
    virtual void setSkipFramesToTime(quint64 skipFramesToTime);

protected:
	quint64 m_lengthMksec;
    quint64 m_startMksec;

	bool m_forward;
	bool m_singleShot;
	CLAdaptiveSleep m_adaptiveSleep;
	qint64 m_needToSleep;

	mutable QMutex m_cs;

	bool m_useTwice;

private:
	quint64 m_skipFramesToTime;
};

#endif //abstract_archive_stream_reader_h1907
