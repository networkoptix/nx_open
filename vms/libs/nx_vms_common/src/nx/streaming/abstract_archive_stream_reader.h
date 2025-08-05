// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/streaming/abstract_archive_delegate.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/streaming/abstract_navigator.h>
#include <nx/string.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/move_only_func.h>

struct QnTimePeriod;
class QnTimePeriodList;
class AbstractArchiveIntegrityWatcher;
class AbstractMediaDataFilter;

class NX_VMS_COMMON_API QnAbstractArchiveStreamReader:
    public QnAbstractMediaStreamDataProvider,
    public QnAbstractNavigator
{
    Q_OBJECT

public:
    QnAbstractArchiveStreamReader(const QnResourcePtr& resource);
    virtual ~QnAbstractArchiveStreamReader();

    QnAbstractNavigator *navDelegate() const;
    void setNavDelegate(QnAbstractNavigator* navDelegate);

    virtual QnAbstractArchiveDelegate* getArchiveDelegate() const = 0;
    virtual std::unique_ptr<QnAbstractArchiveDelegate> releaseArchiveDelegate() = 0;

    virtual bool isSingleShotMode() const = 0;

    // Manual open. Open will be called automatically on first data access
    virtual bool open(AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher) = 0;

    virtual bool isSkippingFrames() const = 0;

    void jumpToPreviousFrame(qint64 usec);

    // gives a list of audio tracks
    virtual QStringList getAudioTracksInfo() const;

    //gets current audio channel ( if we have the only channel - returns 0 )
    virtual unsigned getCurrentAudioChannel() const;

    //!Implementation of QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection
    virtual CameraDiagnostics::Result diagnoseMediaStreamConnection() override;

    // sets certain track
    virtual bool setAudioChannel(unsigned num);

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

    virtual bool setStreamDataFilter(nx::vms::api::StreamDataFilters filter) = 0;
    virtual nx::vms::api::StreamDataFilters streamDataFilter() const = 0;
    virtual void setStorageLocationFilter(nx::vms::api::StorageLocation filter) = 0;
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

    virtual void setArchiveDelegate(std::unique_ptr<QnAbstractArchiveDelegate> delegate) = 0;

    virtual void run() override;

    virtual double getSpeed() const override = 0;

    virtual void startPaused(qint64 startTime) = 0;
    virtual void setGroupId(const nx::String& groupId)  = 0;

    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool value) { m_enabled = value; }
    virtual void setEndOfPlaybackHandler(std::function<void()> /*handler*/) {}
    virtual void setErrorHandler(std::function<void(const QString& errorString)> /*handler*/) {}
    void setNoDataHandler(nx::MoveOnlyFunc<void()> noDataHandler);

    void addMediaFilter(const std::shared_ptr<AbstractMediaDataFilter>& filter);

    /**
     * \returns                         Current position of this reader, in
     *                                  microseconds.
     */
    virtual std::chrono::microseconds currentTime() const = 0;

    virtual bool jumpToEx(qint64 usec, qint64 skipTime, bool bindPositionToPlaybackMask, qint64* outJumpTime, bool useDelegate = true) = 0;
    virtual bool isJumpProcessing() const = 0;

    void setRealtimeSpeed(float value) { m_realTimeSpeed = value; }

protected:
    void resetRealtimeDelay();

    virtual QnAbstractMediaDataPtr getNextData() = 0;
signals:
    void beforeJump(qint64 usec);
    void jumpOccurred(qint64 usec, int sequence);
    void streamAboutToBePaused();
    void streamPaused();
    void streamAboutToBeResumed();
    void streamResumed();
    void nextFrameOccurred();
    void prevFrameOccurred();
    void skipFramesTo(qint64 usec);
    void waitForDataCanBeAccepted();

protected:
    bool m_cycleMode = false;
    qint64 m_needToSleep = 0;
    QnAbstractNavigator* m_navDelegate = nullptr;
    nx::MoveOnlyFunc<void()> m_noDataHandler;

private:
    std::chrono::milliseconds getDelay(int64_t timestamp);
private:
    // For realtime delay.
    std::optional<float> m_realTimeSpeed{};
    nx::utils::ElapsedTimer m_timer; //< Timer for realtime playback delay.
    int64_t m_sendTimeBaseUs = -1;

    bool m_enabled = true;
    std::vector<std::shared_ptr<AbstractMediaDataFilter>> m_filters;
};

using QnAbstractArchiveStreamReaderPtr = std::shared_ptr<QnAbstractArchiveStreamReader>;
