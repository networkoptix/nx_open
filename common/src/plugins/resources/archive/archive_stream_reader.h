#ifndef avi_stream_reader_h1901
#define avi_stream_reader_h1901

#include <QWaitCondition>
#include "abstract_archive_stream_reader.h"
#include "core/resource/resource_media_layout.h"
#include "utils/media/ffmpeg_helper.h"
#include <libavformat/avformat.h>
#include "playbackmask_helper.h"

struct AVFormatContext;

class QnArchiveStreamReader : public QnAbstractArchiveReader
{
    Q_OBJECT;

public:
    QnArchiveStreamReader(QnResourcePtr dev);
    virtual ~QnArchiveStreamReader();

    virtual bool isSkippingFrames() const;
    virtual qint64 currentTime() const;

    virtual QStringList getAudioTracksInfo() const;
    virtual unsigned int getCurrentAudioChannel() const;
    virtual bool setAudioChannel(unsigned int num);
    virtual void setReverseMode(bool value);
    virtual bool isReverseMode() const { return m_reverseMode;}
    virtual bool isNegativeSpeedSupported() const;
    virtual void setSingleShotMode(bool single);
    virtual bool isSingleShotMode() const;


    virtual const QnVideoResourceLayout* getDPVideoLayout() const;
    virtual const QnResourceAudioLayout* getDPAudioLayout() const;
    static bool deserializeLayout(CLCustomDeviceVideoLayout* layout, const QString& layoutStr);
    static QString serializeLayout(const QnVideoResourceLayout* layout);
    void renameFileOnDestroy(const QString& newFileName);
    void jumpWithMarker(qint64 mksec, bool findIFrame, int marker);

    // jump to frame directly ignoring start of GOP
    virtual void directJumpToNonKeyFrame(qint64 mksec);

    virtual bool jumpTo(qint64 mksec, qint64 skipTime);
    virtual void nextFrame();
    void needMoreData();
    virtual void previousFrame(qint64 mksec);
    virtual void pauseMedia();
    virtual bool isMediaPaused() const;
    virtual void resumeMedia();
    virtual QnAbstractMediaDataPtr getNextData();

    bool setMotionRegion(const QRegion& region);
    bool setSendMotion(bool value);

    void setPlaybackMask(const QnTimePeriodList& playbackMask);
    virtual void setQuality(MediaQuality quality, bool fastSwitch) override;
    virtual void disableQualityChange() override;
    virtual void enableQualityChange() override;

    /* For atomic changing several params: quality and position for example */
    void lock();
    void unlock();
protected:
    virtual bool init();

    virtual ByteIOContext* getIOContext();

    virtual qint64 contentLength() const { return m_delegate->endTime() - m_delegate->startTime(); }
    bool initCodecs();
    bool openFormatContext();
    void setCurrentTime(qint64 value);
    virtual void pleaseStop();
    QnAbstractMediaDataPtr createEmptyPacket(bool isReverseMode);
	void beforeJumpInternal(qint64 mksec);
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
    int m_selectedAudioChannel;
    bool m_eof;
    bool m_reverseMode;
    bool m_prevReverseMode;
    FrameTypeExtractor* m_frameTypeExtractor;
    qint64 m_lastGopSeekTime;
    //QVector<qint64> m_lastPacketTimes;
    QVector<int> m_audioCodecs;
    bool m_IFrameAfterJumpFound;
    qint64 m_requiredJumpTime;
    qint64 m_lastFrameDuration;
    QString m_onDestroyFileName;
    bool m_BOF;
    qint64 m_BOFTime;
    int m_dataMarker;
    int m_newDataMarker;

private:
    bool m_canChangeQuality;
    bool m_externalLocked;
    bool m_exactJumpToSpecifiedFrame;
    bool m_ignoreSkippingFrame;
    qint64 m_lastJumpTime;
    qint64 m_skipFramesToTime;
    bool m_keepLastSkkipingFrame;
    bool m_singleShot;
    bool m_singleQuantProcessed;
    mutable QMutex m_jumpMtx;
    QWaitCondition m_singleShowWaitCond;
    QnAbstractMediaDataPtr m_currentData;
    QnAbstractMediaDataPtr m_nextData;
    QQueue<QnAbstractMediaDataPtr> m_skippedMetadata;

    QMutex m_playbackMaskSync;
    QnPlaybackMaskHelper m_playbackMaskHelper;
    MediaQuality m_quality;
    bool m_qualityFastSwitch;
    MediaQuality m_oldQuality;
    bool m_oldQualityFastSwitch;

    qint64 determineDisplayTime();
    void intChanneljumpTo(qint64 mksec, int channel);
    bool getNextVideoPacket();
    void addAudioChannel(QnCompressedAudioDataPtr audio);
    QnAbstractMediaDataPtr getNextPacket();
    void channeljumpToUnsync(qint64 mksec, int channel, qint64 skipTime);
    void setSkipFramesToTime(qint64 skipFramesToTime, bool keepLast);
};

#endif //avi_stream_reader_h1901
