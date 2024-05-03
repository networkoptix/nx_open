// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QQueue>

#include <nx/streaming/abstract_archive_stream_reader.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/vms/api/types/storage_location.h>
#include <recording/playbackmask_helper.h>

#include "motion/metadata_multiplexer.h"

// Private.
class FrameTypeExtractor;

/**
 * Provides reading of media data from QnMediaResource stream with specified playback speed and
 * with navigation supported.
 * Before starting to work, setArchiveDelegate() must be called, passing QnAbstractArchiveDelegate
 * working on the same QnMediaResource. Most of QnAbstractArchiveDelegate implementations
 * require pointer to QnAbstractArchiveStreamReader (or subclass) passed into constructor, so
 * cross-referencing will be established this way.
 * To consume data read, user has to subclass QnAbstractDataConsumer and provide one or more
 * instances to addDataProcessor(). Sequential calls for QnAbstractDataConsumer::processData()
 * will happen passing sequential packets of data read in form of QnAbstractMediaDataPtr.
 * Video frames are kept compressed, so decoding is up to user.
 * Allows to set playback speed, including negative, i.e. reverse playback is supported.
 * Navigation supported includes pausing and resuming playback, getting next and previous frames,
 * jumping to specific position (with the most of key frame related routines encapsulated).
 * It is possible to set single (via setPlaybackRange()) or multiple (via setPlaybackMask()) time
 * intervals where playback is exclusively permitted.
 */
class NX_VMS_COMMON_API QnArchiveStreamReader: public QnAbstractArchiveStreamReader
{
    Q_OBJECT
    using base_type = QnAbstractArchiveStreamReader;

public:
    /**
     * @param dev Must be an instance of QnMediaResource subclass.
     */
    QnArchiveStreamReader(const QnResourcePtr& dev);

    virtual ~QnArchiveStreamReader() override;

    virtual bool isSkippingFrames() const override;

    virtual QStringList getAudioTracksInfo() const override;
    virtual unsigned getCurrentAudioChannel() const override;
    virtual bool setAudioChannel(unsigned num) override;
    virtual bool isReverseMode() const override;

    /** Is not used and not implemented. */
    virtual bool isNegativeSpeedSupported() const override;

    virtual bool isSingleShotMode() const override;

    virtual QnConstResourceVideoLayoutPtr getDPVideoLayout() const;
    virtual AudioLayoutConstPtr getDPAudioLayout() const;
    virtual bool hasVideo() const override;
    void renameFileOnDestroy(const QString& newFileName);
    //void jumpWithMarker(qint64 mksec, bool findIFrame, int marker);
    void setMarker(int marker);

    /**
     * Jump to frame directly, ignoring start of GOP, i.e. no preliminary jump to preceding key
     * frame performed, so usually no possibility to correctly decode the frame.
     */
    virtual void directJumpToNonKeyFrame(qint64 mksec) override;

    /** The same as jumpToEx() with bindPositionToPlaybackMask = true. */
    virtual bool jumpTo(qint64 mksec, qint64 skipTime) override;

    /**
     * Jump to frame at time specified by mksec. Actually, preliminary jump to preceding key
     * frame performed, and all intermediate frames are read to allow normal decoding. All such
     * intermediate frames get MediaFlags_Ignore flag set. It is guaranteed that frames read are
     * enough to decode frames up to including mksec.
     * Note: jumping (forward or backward) within the GOP of current position causes
     * preliminary jump to preceding key frame again, despite it is not necessary for decoding.
     * Known bug: requesting a jump to first 1-4 (approximately) frames just after key frame
     * results in actual jump to the key frame and no further. The reason is not known yet.
     * @param mksec Time to jump in microseconds.
     * @param skipTime During jump, skip frames until skipTime provided, microseconds. Usual
     *     usage is to pass skipTime equal to mksec.
     * @param bindPositionToPlaybackMask Consider time interval provided to setPlaybackRange()
     *     and setPlaybackMask(); i.e. if true, jump outside them is not possible.
     *     If false, jumping outside playback range triggers reading all frames from start
     *     of GOP to the frame specified by mksec and pausing the media just after reaching.
     * @param outJumpTime If not nullptr, actual jump time (after correction by playback mask),
     *     in microseconds, will be written by the pointer.
     * @param useDelegate If false, QnAbstractNavigator object provided to setNavDelegate() will
     *     not be used during jump.
     * @return false if jump to the same mksec and skipTime is already performing right now.
     */
    virtual bool jumpToEx(qint64 mksec, qint64 skipTime, bool bindPositionToPlaybackMask, qint64* outJumpTime, bool useDelegate = true) override;

    virtual void setSkipFramesToTime(qint64 skipTime) override;
    virtual void nextFrame() override;
    void needMoreData();
    virtual void previousFrame(qint64 mksec) override;
    virtual void pauseMedia() override;
    virtual bool isMediaPaused() const override;
    virtual void resumeMedia() override;
    virtual QnAbstractMediaDataPtr getNextData() override;

    virtual bool setStreamDataFilter(nx::vms::api::StreamDataFilters filter) override;
    virtual nx::vms::api::StreamDataFilters streamDataFilter() const override;

    virtual void setStorageLocationFilter(nx::vms::api::StorageLocation filter) override;

    /**
     * Set time period within the archive, where playback is permitted. Upon reaching borders
     * of playbackRange, playback is paused. If no frames were read by the object yet, also sets
     * current position to the start of playbackRange specified. Other aspects of behavior are the
     * same as for setPlaybackMask().
     */
    virtual void setPlaybackRange(const QnTimePeriod& playbackRange) override;

    virtual QnTimePeriod getPlaybackRange() const override;

    /**
     * Set several time periods within the archive, where playback is permitted. Upon reaching
     * a border of a period, playback is paused. Never changes current playback position.
     * There is the only way to read any frames outside playback mask: call jumpToEx() passing
     * bindPositionToPlaybackMask = false. All other navigation methods will never cause reading
     * frames outside playback mask.
     */
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

    virtual CodecParametersConstPtr getCodecContext() const override;

    virtual void startPaused(qint64 startTime) override;

    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;

    /* Return true if archive range is accessible immediately without opening an archive */
    bool offlineRangeSupported() const;

    virtual void afterRun() override;
    virtual void setGroupId(const nx::String& groupId) override;

    virtual void pause() override;
    virtual void resume() override;
    virtual bool isPaused() const override;

    virtual bool isRealTimeSource() const override;
    virtual void pleaseStop() override;

    virtual void setEndOfPlaybackHandler(std::function<void()> handler) override;
    virtual void setErrorHandler(
        std::function<void(const QString& errorString)> handler) override;

    CameraDiagnostics::Result lastError() const;

    virtual std::chrono::microseconds currentTime() const override;

    void reopen();
    bool isJumpProcessing() const;

protected:
    virtual bool init();

    virtual qint64 contentLength() const { return m_delegate->endTime() - m_delegate->startTime(); }
    bool initCodecs();
    bool openFormatContext();
    void setCurrentTime(qint64 value);
    QnAbstractMediaDataPtr createEmptyPacket(bool isReverseMode);
    void beforeJumpInternal(qint64 mksec);

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
    virtual bool needKeyData(int channel) const override;
    void emitJumpOccurred(qint64 jumpTime, bool usePreciseSeek, int sequence);

private slots:
private:
    unsigned m_selectedAudioChannel;
    bool m_eof;
    FrameTypeExtractor* m_frameTypeExtractor;
    qint64 m_lastGopSeekTime;
    QVector<int> m_audioCodecs;
    bool m_IFrameAfterJumpFound;
    qint64 m_requiredJumpTime;
    qint64 m_lastSeekPosition = AV_NOPTS_VALUE;
    bool m_lastUsePreciseSeek;
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
    qint64 m_skipFramesToTime;
    bool m_keepLastSkkipingFrame;
    bool m_singleShot;
    bool m_singleQuantProcessed;
    mutable nx::Mutex m_jumpMtx;
    nx::WaitCondition m_singleShowWaitCond;
    QnAbstractMediaDataPtr m_currentData;
    QnAbstractMediaDataPtr m_afterMotionData;
    QnAbstractMediaDataPtr m_nextData;
    QQueue<QnAbstractMediaDataPtr> m_skippedMetadata;
    CodecParametersConstPtr m_codecContext;

    mutable nx::Mutex m_playbackMaskSync;
    QnPlaybackMaskHelper m_playbackMaskHelper;
    MediaQuality m_quality;
    bool m_qualityFastSwitch;
    QSize m_customResolution;
    MediaQuality m_oldQuality;
    bool m_oldQualityFastSwitch;
    QSize m_oldResolution;
    bool m_isStillImage;
    double m_speed = 0.0f;
    double m_prevSpeed;

    bool m_rewSecondaryStarted[CL_MAX_CHANNELS];
    std::shared_ptr<MetadataMultiplexer> m_motionConnection[CL_MAX_CHANNELS];
    nx::vms::api::StreamDataFilters m_streamDataFilter;
    nx::vms::api::StreamDataFilters m_prevStreamDataFilter;

    nx::vms::api::StorageLocation m_prevStorageLocationFilter{};

    bool m_outOfPlaybackMask;
    qint64 m_latPacketTime;

    bool m_stopCond;
    mutable nx::Mutex m_stopMutex;
    nx::WaitCondition m_stopWaitCond;
    mutable std::optional<bool> m_hasVideo;

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
