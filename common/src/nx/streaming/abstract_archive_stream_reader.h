#ifndef ABSTRACT_ARCHIVE_STREAM_READER_H
#define ABSTRACT_ARCHIVE_STREAM_READER_H

#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/streaming/abstract_archive_delegate.h>
#include <nx/streaming/abstract_navigator.h>

#include <nx/utils/move_only_func.h>

class QnTimePeriod;
class QnTimePeriodList;
class AbstractArchiveIntegrityWatcher;

class QnAbstractArchiveStreamReader: public QnAbstractMediaStreamDataProvider, public QnAbstractNavigator
{
    Q_OBJECT;

public:
    QnAbstractArchiveStreamReader(const QnResourcePtr& dev);
    virtual ~QnAbstractArchiveStreamReader();

    QnAbstractNavigator *navDelegate() const;
    void setNavDelegate(QnAbstractNavigator* navDelegate);

    QnAbstractArchiveDelegate* getArchiveDelegate() const;

    virtual bool isSingleShotMode() const = 0;


    // Manual open. Open will be called automatically on first data access
    bool open(AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher);

    virtual bool isSkippingFrames() const = 0;

    /**
     * \returns                         Length of archive, in microseconds.
     */
    quint64 lengthUsec() const;

    virtual void directJumpToNonKeyFrame(qint64 usec) = 0;
    virtual void setSkipFramesToTime(qint64 skipTime) = 0;
    void jumpToPreviousFrame(qint64 usec);


    // gives a list of audio tracks
    virtual QStringList getAudioTracksInfo() const;

    //gets current audio channel ( if we have the only channel - returns 0 )
    virtual unsigned int getCurrentAudioChannel() const;

    //!Implementation of QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection
    virtual CameraDiagnostics::Result diagnoseMediaStreamConnection() override;

    // sets certain track
    virtual bool setAudioChannel(unsigned int num);

    /** Is not used and not implemented. */
    virtual bool isNegativeSpeedSupported() const = 0;

    void setCycleMode(bool value);

    /**
     * \returns                         Time of the archive's start, in microseconds.
     */
    virtual qint64 startTime() const = 0;

    /**
     * \returns                         Time of the archive's end, in microseconds.
     */
    virtual qint64 endTime() const = 0;

    virtual bool isRealTimeSource() const = 0;

    virtual bool setSendMotion(bool value) = 0;
    virtual void setPlaybackMask(const QnTimePeriodList& playbackMask) = 0;
    virtual void setPlaybackRange(const QnTimePeriod& playbackRange) = 0;
    virtual QnTimePeriod getPlaybackRange() const = 0;

    /**
     * @param resolution Should be specified only if quality == MEDIA_Quality_CustomResolution;
     *     width can be <= 0 which is treated as "auto".
     */
    virtual void setQuality(MediaQuality quality, bool fastSwitch, const QSize& resolution = QSize()) = 0;

    virtual MediaQuality getQuality() const = 0;
    virtual AVCodecID getTranscodingCodec() const = 0;

    virtual void setArchiveDelegate(QnAbstractArchiveDelegate* contextDelegate) = 0;

    virtual void run() override;

    virtual void setSpeed(double value, qint64 currentTimeHint = AV_NOPTS_VALUE) = 0;
    virtual double getSpeed() const = 0;

    virtual void startPaused(qint64 startTime) = 0;
    virtual void setGroupId(const QByteArray& groupId)  = 0;

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool value) { m_enabled = value; }
    virtual void setEndOfPlaybackHandler(std::function<void()> /*handler*/) {}
    virtual void setErrorHandler(std::function<void(const QString& errorString)> /*handler*/) {}
    void setNoDataHandler(nx::utils::MoveOnlyFunc<void()> noDataHandler);
protected:

    /**
     * \returns                         Current position of this reader, in
     *                                  microseconds.
     */
    virtual qint64 currentTime() const = 0;

    virtual QnAbstractMediaDataPtr getNextData() = 0;
signals:
    void beforeJump(qint64 mksec);
    void jumpOccured(qint64 mksec, int sequence);
    void jumpCanceled(qint64 mksec);
    void streamAboutToBePaused();
    void streamPaused();
    void streamAboutToBeResumed();
    void streamResumed();
    void nextFrameOccured();
    void prevFrameOccured();
    void skipFramesTo(qint64 mksec);
protected:
    AbstractArchiveIntegrityWatcher* m_archiveIntegrityWatcher = nullptr;
    bool m_cycleMode;
    qint64 m_needToSleep;
    QnAbstractArchiveDelegate* m_delegate;
    QnAbstractNavigator* m_navDelegate;
    nx::utils::MoveOnlyFunc<void()> m_noDataHandler;
private:
    bool m_enabled;
};

#endif // ABSTRACT_ARCHIVE_STREAM_READER_H
