#pragma once

#include <QtCore/QQueue>

#include <core/resource/resource_media_layout.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/media_context.h>

#include <nx/utils/thread/wait_condition.h>
#include <recording/playbackmask_helper.h>

#include "motion/metadata_multiplexer.h"

// Private.
class FrameTypeExtractor;

class QnArchiveStreamReader: public QnAbstractArchiveStreamReader
{
    Q_OBJECT
    using base_type = QnAbstractArchiveStreamReader;

public:
    QnArchiveStreamReader(const QnResourcePtr& dev);
    virtual ~QnArchiveStreamReader() override;

    virtual bool isSkippingFrames() const override;

    virtual QStringList getAudioTracksInfo() const override;
    virtual unsigned getCurrentAudioChannel() const override;
    virtual bool setAudioChannel(unsigned num) override;
    virtual bool isReverseMode() const override { return m_speed < 0;}

    /** Is not used and not implemented. */
    virtual bool isNegativeSpeedSupported() const override;

    virtual bool isSingleShotMode() const override;

    virtual QnConstResourceVideoLayoutPtr getDPVideoLayout() const;
    virtual QnConstResourceAudioLayoutPtr getDPAudioLayout() const;
    virtual bool hasVideo() const override;
    void renameFileOnDestroy(const QString& newFileName);
    //void jumpWithMarker(qint64 mksec, bool findIFrame, int marker);
    void setMarker(int marker);

    // jump to frame directly ignoring start of GOP
    virtual void directJumpToNonKeyFrame(qint64 mksec) override;

    virtual bool jumpTo(qint64 mksec, qint64 skipTime) override;

    // Function jumps to the specified time or to the corrected accordingly to the playback
    // mask position.
    bool jumpTo(qint64 mksec, qint64 skipTime, bool bindPositionToPlaybackMask, qint64* outJumpTime);

    virtual void setSkipFramesToTime(qint64 skipTime) override;
    virtual void nextFrame() override;
    void needMoreData();
    virtual void previousFrame(qint64 mksec) override;
    virtual void pauseMedia() override;
    virtual bool isMediaPaused() const override;
    virtual void resumeMedia() override;
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual bool needKeyData(int channel) const override;

    virtual bool setStreamDataFilter(nx::vms::api::StreamDataFilters filter) override;
    virtual nx::vms::api::StreamDataFilters streamDataFilter() const override;

    virtual void setPlaybackRange(const QnTimePeriod& playbackRange) override;
    virtual QnTimePeriod getPlaybackRange() const override;
    virtual void setPlaybackMask(const QnTimePeriodList& playbackMask) override;
    virtual void setQuality(MediaQuality quality, bool fastSwitch, const QSize& resolution = QSize()) override;
    virtual MediaQuality getQuality() const override;
    virtual AVCodecID getTranscodingCodec() const override;

    virtual void setSpeed(double value, qint64 currentTimeHint = AV_NOPTS_VALUE) override;
    virtual double getSpeed() const override;

    /* For atomic changing several params: quality and position for example */
    void lock();
    void unlock();

    virtual void setArchiveDelegate(QnAbstractArchiveDelegate* contextDelegate) override;

    virtual QnConstMediaContextPtr getCodecContext() const override;

    virtual void startPaused(qint64 startTime) override;

    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;

    /* Return true if archive range is accessible immediately without opening an archive */
    bool offlineRangeSupported() const;

    virtual void afterRun() override;
    virtual void setGroupId(const QByteArray& groupId) override;

    virtual void pause() override;
    virtual void resume() override;
    virtual bool isPaused() const override;

    virtual bool isRealTimeSource() const override;
    virtual void pleaseStop() override;

    virtual void setEndOfPlaybackHandler(std::function<void()> handler) override;
    virtual void setErrorHandler(
        std::function<void(const QString& errorString)> handler) override;
    bool isOpened() const;

    CameraDiagnostics::Result lastError() const;

protected:
    virtual bool init();

    virtual qint64 contentLength() const { return m_delegate->endTime() - m_delegate->startTime(); }
    bool initCodecs();
    bool openFormatContext();
    void setCurrentTime(qint64 value);
    QnAbstractMediaDataPtr createEmptyPacket(bool isReverseMode);
    void beforeJumpInternal(qint64 mksec);

    virtual qint64 currentTime() const override;
protected:
    qint64 m_currentTime;
    qint64 m_topIFrameTime;
    qint64 m_bottomIFrameTime;


    int m_primaryVideoIdx;
    int m_audioStreamIndex;

    AVCodecID m_videoCodecId;
    AVCodecID m_audioCodecId;

    int m_freq;
    int m_channels;

    bool m_firstTime;

    volatile bool m_wakeup;
    qint64 m_tmpSkipFramesToTime;
private:
    void setSpeedInternal(double speed, qint64 currentTimeHint = AV_NOPTS_VALUE);
    bool isCompatiblePacketForMask(const QnAbstractMediaDataPtr& mediaData) const;
private slots:
private:
    unsigned m_selectedAudioChannel;
    bool m_eof;
    FrameTypeExtractor* m_frameTypeExtractor;
    qint64 m_lastGopSeekTime;
    QVector<int> m_audioCodecs;
    bool m_IFrameAfterJumpFound;
    qint64 m_requiredJumpTime;
    QString m_onDestroyFileName;
    bool m_BOF;
    int m_afterBOFCounter;
    int m_dataMarker;
    int m_newDataMarker;
    qint64 m_currentTimeHint;

private:
    bool m_bofReached;
    bool m_externalLocked;
    bool m_exactJumpToSpecifiedFrame;
    bool m_ignoreSkippingFrame;
    qint64 m_lastJumpTime;
    qint64 m_lastSkipTime;
    qint64 m_skipFramesToTime;
    bool m_keepLastSkkipingFrame;
    bool m_singleShot;
    bool m_singleQuantProcessed;
    mutable QnMutex m_jumpMtx;
    QnWaitCondition m_singleShowWaitCond;
    QnAbstractMediaDataPtr m_currentData;
    QnAbstractMediaDataPtr m_afterMotionData;
    QnAbstractMediaDataPtr m_nextData;
    QQueue<QnAbstractMediaDataPtr> m_skippedMetadata;
    QnConstMediaContextPtr m_codecContext;

    mutable QnMutex m_playbackMaskSync;
    QnPlaybackMaskHelper m_playbackMaskHelper;
    MediaQuality m_quality;
    bool m_qualityFastSwitch;
    QSize m_customResolution;
    MediaQuality m_oldQuality;
    bool m_oldQualityFastSwitch;
    QSize m_oldResolution;
    bool m_isStillImage;
    double m_speed;
    double m_prevSpeed;

    bool m_rewSecondaryStarted[CL_MAX_CHANNELS];
    std::shared_ptr<MetadataMultiplexer> m_motionConnection[CL_MAX_CHANNELS];
    bool m_pausedStart;
    nx::vms::api::StreamDataFilters m_streamDataFilter;
    nx::vms::api::StreamDataFilters m_prevStreamDataFilter;
    bool m_outOfPlaybackMask;
    qint64 m_latPacketTime;

    bool m_stopCond;
    mutable QnMutex m_stopMutex;
    QnWaitCondition m_stopWaitCond;
    mutable boost::optional<bool> m_hasVideo;

    qint64 determineDisplayTime(bool reverseMode);
    void internalJumpTo(qint64 mksec);
    bool getNextVideoPacket();
    QnAbstractMediaDataPtr getNextPacket();
    void channeljumpToUnsync(qint64 mksec, int channel, qint64 skipTime);
    void setSkipFramesToTime(qint64 skipFramesToTime, bool keepLast);
    std::function<void()> m_endOfPlaybackHandler;
    std::function<void(const QString& errorString)> m_errorHandler;

    void updateMetadataReaders(int channel, nx::vms::api::StreamDataFilters filter);
};
