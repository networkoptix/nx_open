#ifndef abstract_archive_stream_reader_h1907
#define abstract_archive_stream_reader_h1907

#include "abstract_archive_delegate.h"
#include "utils/common/adaptivesleep.h"
#include "core/dataprovider/cpull_media_stream_provider.h"

class QnAbstractNavigator
{
public:
    virtual bool jumpTo(qint64 mksec,  qint64 skipTime) = 0;
    virtual void previousFrame(qint64 mksec) = 0;
    virtual void nextFrame() = 0;
    virtual void pauseMedia() = 0;
    virtual void resumeMedia() = 0;
    virtual void setSingleShotMode(bool single) = 0;

    virtual bool isMediaPaused() const = 0;

    // playback filter by motion detection mask
    //virtual bool setMotionRegion(const QRegion& region) = 0;

    // delivery motion data to a client
    //virtual bool setSendMotion(bool value) = 0;
};

class QnAbstractArchiveReader : public QnClientPullMediaStreamProvider, public QnAbstractNavigator
{
    Q_OBJECT
public:
    QnAbstractArchiveReader(QnResourcePtr dev);
    virtual ~QnAbstractArchiveReader();

    void setArchiveDelegate(QnAbstractArchiveDelegate* contextDelegate);
    void setNavDelegate(QnAbstractNavigator* navDelegate);

    QnAbstractArchiveDelegate* getArchiveDelegate() const;

    virtual bool isSingleShotMode() const = 0;


    // Manual open. Open will be called automatically on first data access
    bool open();

    virtual qint64 currentTime() const = 0;
	virtual bool isSkippingFrames() const = 0;

    /**
      * @return length of archive in mksec
      */
    quint64 lengthMksec() const;

    void jumpToPreviousFrame(qint64 mksec);


    // gives a list of audio tracks
    virtual QStringList getAudioTracksInfo() const;

    //gets current audio channel ( if we have the only channel - returns 0 )
    virtual unsigned int getCurrentAudioChannel() const;

    // sets certain track
    virtual bool setAudioChannel(unsigned int num);
    virtual bool isNegativeSpeedSupported() const = 0;

    void setCycleMode(bool value);

    qint64 startTime() const;
    qint64 endTime() const;
    bool isRealTimeSource() const;

    virtual bool setMotionRegion(const QRegion& region) = 0;
    virtual bool setSendMotion(bool value) = 0;

Q_SIGNALS:
    void beforeJump(qint64 mksec);
    void jumpOccured(qint64 mksec);
    void jumpCanceled(qint64 mksec);
    void streamPaused();
    void streamResumed();
    void nextFrameOccured();
    void prevFrameOccured();
protected:
    bool m_cycleMode;
    qint64 m_needToSleep;
    QnAbstractArchiveDelegate* m_delegate;
    QnAbstractNavigator* m_navDelegate;

};

#endif //abstract_archive_stream_reader_h1907
