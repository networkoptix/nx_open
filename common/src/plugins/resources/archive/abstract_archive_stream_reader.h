#ifndef abstract_archive_stream_reader_h1907
#define abstract_archive_stream_reader_h1907


#include "abstract_archive_delegate.h"
#include "utils/common/adaptivesleep.h"
#include "core/dataprovider/cpull_media_stream_provider.h"

class QnAbstractArchiveReader : public QnClientPullMediaStreamProvider
{
    Q_OBJECT
public:
	QnAbstractArchiveReader(QnResourcePtr dev);
	virtual ~QnAbstractArchiveReader();

    void setArchiveDelegate(QnAbstractArchiveDelegate* contextDelegate);
    QnAbstractArchiveDelegate* getArchiveDelegate() const;

	        void setSingleShotMode(bool single);
	        bool isSingleShotMode() const;


    // Manual open. Open will be called automatically on first data access
    bool open();

	virtual qint64 currentTime() const = 0;
	bool isSkippingFrames() const;

	/**
      * @return length of archive in mksec
      */
	quint64 lengthMksec() const;

    virtual void jumpTo(qint64 mksec, bool makeshot, qint64 skipTime = 0);
	void jumpToPreviousFrame(qint64 mksec, bool makeshot);

    virtual void previousFrame(qint64 /*mksec*/);
    virtual void nextFrame();


    // gives a list of audio tracks 
    virtual QStringList getAudioTracksInfo() const;

    //gets current audio channel ( if we have the only channel - returns 0 )
    virtual unsigned int getCurrentAudioChannel() const;

    // sets certain track 
    virtual bool setAudioChannel(unsigned int num);
    virtual bool isNegativeSpeedSupported() const = 0;

    void setCycleMode(bool value);

    virtual void pause();
    void resumeMedia();

    qint64 skipFramesToTime() const;
    qint64 startTime() const;
    qint64 endTime() const;
    bool isRealTimeSource() const;
signals:
    void singleShotModeChanged(bool value);
    void beforeJump(qint64 mksec, bool makeshot);
    void jumpOccured(qint64 mksec);
    void streamPaused();
    void streamResumed();
    void nextFrameOccured();
    void prevFrameOccured();
protected:
    virtual void resume();

    virtual void setSkipFramesToTime(qint64 skipFramesToTime);
	virtual void channeljumpTo(qint64 mksec, int channel, qint64 borderTime) = 0;
    virtual void pleaseStop();
protected:
    bool m_cycleMode;
	quint64 m_lengthMksec;

	bool m_singleShot;
	CLAdaptiveSleep m_adaptiveSleep;
	qint64 m_needToSleep;

	mutable QMutex m_cs;
    mutable QMutex m_framesMutex;

    QnAbstractArchiveDelegate* m_delegate;
    qint64 m_skipFramesToTime;
private:
    qint64 m_lastJumpTime;
};

#endif //abstract_archive_stream_reader_h1907
