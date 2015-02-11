#ifndef avi_stream_reader_h1901
#define avi_stream_reader_h1901

#ifdef ENABLE_ARCHIVE

extern "C"
{
    #include <libavformat/avformat.h>
}

#include <utils/common/wait_condition.h>

#include "core/datapacket/audio_data_packet.h"
#include "core/resource/resource_media_layout.h"

#include <recording/playbackmask_helper.h>

#include "utils/media/ffmpeg_helper.h"

#include "abstract_archive_stream_reader.h"


struct AVFormatContext;

class QnArchiveStreamReader : public QnAbstractArchiveReader
{
    Q_OBJECT;

public:
    QnArchiveStreamReader(const QnResourcePtr& dev);
    virtual ~QnArchiveStreamReader();

    virtual bool isSkippingFrames() const;

    virtual QStringList getAudioTracksInfo() const;
    virtual unsigned int getCurrentAudioChannel() const;
    virtual bool setAudioChannel(unsigned int num);
    virtual bool isReverseMode() const { return m_reverseMode;}
    virtual bool isNegativeSpeedSupported() const;
    virtual bool isSingleShotMode() const;


    virtual QnConstResourceVideoLayoutPtr getDPVideoLayout() const;
    virtual QnConstResourceAudioLayoutPtr getDPAudioLayout() const;
    static bool deserializeLayout(QnCustomResourceVideoLayout* layout, const QString& layoutStr);
    static QString serializeLayout(const QnResourceVideoLayout* layout);
    void renameFileOnDestroy(const QString& newFileName);
    //void jumpWithMarker(qint64 mksec, bool findIFrame, int marker);
    void setMarker(int marker);

    // jump to frame directly ignoring start of GOP
    virtual void directJumpToNonKeyFrame(qint64 mksec);

    virtual bool jumpTo(qint64 mksec, qint64 skipTime) override;
    virtual void setSkipFramesToTime(qint64 skipTime) override;
    virtual void nextFrame();
    void needMoreData();
    virtual void previousFrame(qint64 mksec);
    virtual void pauseMedia();
    virtual bool isMediaPaused() const;
    virtual void resumeMedia();
    virtual QnAbstractMediaDataPtr getNextData();

    bool setSendMotion(bool value);

    virtual void setPlaybackRange(const QnTimePeriod& playbackRange) override;
    virtual QnTimePeriod getPlaybackRange() const override;
    virtual void setPlaybackMask(const QnTimePeriodList& playbackMask) override;
    virtual void setQuality(MediaQuality quality, bool fastSwitch) override;
    virtual MediaQuality getQuality() const override;

    virtual void setSpeed(double value, qint64 currentTimeHint = AV_NOPTS_VALUE) override;
    virtual double getSpeed() const override;


    /* For atomic changing several params: quality and position for example */
    void lock();
    void unlock();

    virtual void setArchiveDelegate(QnAbstractArchiveDelegate* contextDelegate) override;

    virtual QnMediaContextPtr getCodecContext() const override;

    virtual void startPaused() override;

    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;
    
    /* Return true if archvie range is accessible immedeatly without opening an archive */
    bool offlineRangeSupported() const;

    virtual void afterRun() override;
    virtual void setGroupId(const QByteArray& groupId) override;

    virtual void pause() override;
    virtual void resume() override;

    virtual bool isRealTimeSource() const override;
protected:
    virtual bool init();

    virtual AVIOContext* getIOContext();

    virtual qint64 contentLength() const { return m_delegate->endTime() - m_delegate->startTime(); }
    bool initCodecs();
    bool openFormatContext();
    void setCurrentTime(qint64 value);
    virtual void pleaseStop();
    QnAbstractMediaDataPtr createEmptyPacket(bool isReverseMode);
    void beforeJumpInternal(qint64 mksec);

    virtual qint64 currentTime() const override;
protected:
    qint64 m_currentTime;
    qint64 m_topIFrameTime;
    qint64 m_bottomIFrameTime;


    int m_primaryVideoIdx;
    int m_audioStreamIndex;

    CodecID m_videoCodecId;
    CodecID m_audioCodecId;

    int m_freq;
    int m_channels;

    bool mFirstTime;

    volatile bool m_wakeup;
    qint64 m_tmpSkipFramesToTime;
private:
    void setReverseMode(bool value, qint64 currentTimeHint = AV_NOPTS_VALUE);
private slots:
    void onDelegateChangeQuality(MediaQuality quality);
private:
    int m_selectedAudioChannel;
    bool m_eof;
    bool m_reverseMode;
    bool m_prevReverseMode;
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
    QnMediaContextPtr m_codecContext;

    QnMutex m_playbackMaskSync;
    QnPlaybackMaskHelper m_playbackMaskHelper;
    MediaQuality m_quality;
    bool m_qualityFastSwitch;
    MediaQuality m_oldQuality;
    bool m_oldQualityFastSwitch;
    bool m_isStillImage;
    double m_speed;
    bool m_rewSecondaryStarted[CL_MAX_CHANNELS];
    QnAbstractMotionArchiveConnectionPtr m_motionConnection[CL_MAX_CHANNELS];
    bool m_pausedStart;
    bool m_sendMotion;
    bool m_prevSendMotion;
    bool m_outOfPlaybackMask;
    qint64 m_latPacketTime;
    
    bool m_stopCond;
    QnMutex m_stopMutex;
    QnWaitCondition m_stopWaitCond;

    qint64 determineDisplayTime(bool reverseMode);
    void internalJumpTo(qint64 mksec);
    bool getNextVideoPacket();
    void addAudioChannel(const QnCompressedAudioDataPtr& audio);
    QnAbstractMediaDataPtr getNextPacket();
    void channeljumpToUnsync(qint64 mksec, int channel, qint64 skipTime);
    void setSkipFramesToTime(qint64 skipFramesToTime, bool keepLast);
};

#endif // ENABLE_ARCHIVE

#endif //avi_stream_reader_h1901

