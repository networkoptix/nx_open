// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <deque>
#include <functional>

#include <nx/media/audio_data_packet.h>
#include <nx/media/audio_output.h>
#include <nx/media/media_fwd.h>
#include <nx/media/video_data_packet.h>
#include <nx/streaming/abstract_data_consumer.h>
#include <utils/media/externaltimesource.h>

class QnArchiveStreamReader;

namespace nx {
namespace media {

class AbstractVideoDecoder;
class SeamlessVideoDecoder;
class SeamlessAudioDecoder;

/**
 * Private class used in nx::media::Player.
 */
class NX_MEDIA_API PlayerDataConsumer:
    public QnAbstractDataConsumer,
    public QnlTimeSource
{
    Q_OBJECT
    typedef QnAbstractDataConsumer base_type;

public:
    typedef std::function<QRect()> VideoGeometryAccessor;

public:
    PlayerDataConsumer(const std::unique_ptr<QnArchiveStreamReader>& archiveReader,
        RenderContextSynchronizerPtr renderContextSynchronizer);
    virtual ~PlayerDataConsumer();

    VideoFramePtr dequeueVideoFrame();
    qint64 queueVideoDurationUsec() const;

    ConstAudioOutputPtr audioOutput() const;

    /** Can be Invalid if not available. */
    QSize currentResolution() const;

    /** Can be AV_CODEC_ID_NONE if not available. */
    AVCodecID currentCodec() const;

    std::vector<AbstractVideoDecoder*> currentVideoDecoders() const;

    /** Should be called before other methods; needed by some decoders, e.g. hw-based. */
    void setVideoGeometryAccessor(VideoGeometryAccessor videoGeometryAccessor);

    // ----  QnlTimeSource interface ----

    /**
     * @return Current time.
     * May be different from displayed time.
     * After seek for example, while no any frames are really displayed.
     */
    virtual qint64 getCurrentTime() const override;

    /**
     * @return Last displayed time.
     */
    virtual qint64 getDisplayedTime() const override;
    void setDisplayedTimeUs(qint64 value); //< not part of QnlTimeSource interface

    /**
     * @return Time of the next frame.
     */
    virtual qint64 getNextTime() const override;

    /**
     * @return External clock to sync several video with each other.
     */
    virtual qint64 getExternalTime() const override;

    /** Ask thread to stop. It's a non-blocking call. Thread will be stopped later. */
    virtual void pleaseStop() override;

    /** Turn on / off audio. Allowed to be called from another thread. */
    void setAudioEnabled(bool value);
    bool isAudioEnabled() const;

    void setAllowOverlay(bool value);
    void setAllowHardwareAcceleration(bool value);

    void setPlaySpeed(double value);

    nx::media::StreamEventPacket mediaEvent() const;

signals:
    /** Hint to render to display current data with no delay due to seek operation in progress. */
    void hurryUp();

    /** New video frame is decoded and ready for rendering. */
    void gotVideoFrame();

    /** New audio frame is decoded and ready for play. */
    void gotAudioFrame();

    /** Jump to new position. */
    void jumpOccurred(int sequence);

    void mediaEventChanged();

    /** Got metadata in the media stream. */
    void gotMetadata(QnAbstractCompressedMetadataPtr data);
private slots:
    void onBeforeJump(qint64 timeUsec);
    void onJumpOccurred(qint64 timeUsec, int sequence);

protected:
    virtual bool canAcceptData() const override;
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    virtual void endOfRun() override;
    virtual void clearUnprocessedData() override;

private:
    bool processEmptyFrame(const QnEmptyMediaDataPtr& data);
    bool processVideoFrame(const QnCompressedVideoDataPtr& data);
    bool processAudioFrame(const QnCompressedAudioDataPtr& data);

    void enqueueVideoFrame(VideoFramePtr decodedFrame);
    int getBufferingMask() const;
    QnCompressedVideoDataPtr queueVideoFrame(const QnCompressedVideoDataPtr& videoFrame);
    bool checkSequence(int sequence);

    void updateMediaEvent(const QnAbstractMediaDataPtr& data);

private:
    /**
     * In case of multi-sensor video camera this class is used to calculate
     * which video channels still don't provide frames.
     * Player displays frames without delay unless each channel provides at least 1 frame.
     */
    class MultiSensorHelper
    {
    private:
        int m_mask;
        int m_channels;
    public:
        MultiSensorHelper(): m_mask(0), m_channels(1) {}

        int channelCount() const { return m_channels;  }
        void setChannelCount(int value)
        {
            if (m_channels == value)
                return;
            m_channels = value;
            if (m_mask)
                setMask(); //< update mask for new channels value
        }
        void setMask() { m_mask = (1 << m_channels) - 1; } //< Set m_channels low bits to 1.
        void resetMask() { m_mask = 0; }
        void removeChannel(int channelNumber) { m_mask &= ~(1 << channelNumber); }
        bool hasChannel(int channelNumber) const { return m_mask & (1 << channelNumber);  }
        bool isEmpty() const { return m_mask == 0; }
    };

    typedef std::unique_ptr<SeamlessVideoDecoder> SeamlessVideoDecoderPtr;
    std::vector<SeamlessVideoDecoderPtr> m_videoDecoders;
    std::unique_ptr<SeamlessAudioDecoder> m_audioDecoder;
    AudioOutputPtr m_audioOutput;

    std::deque<VideoFramePtr> m_decodedVideo;
    nx::WaitCondition m_queueWaitCond;
    mutable nx::Mutex m_queueMutex; //< sync with player thread
    mutable nx::Mutex m_jumpMutex; //< sync jump related logic
    mutable nx::Mutex m_decoderMutex; //< sync with create/destroy decoder

    struct BofFrameInfo
    {
        BofFrameInfo(): videoChannel(0), frameNumber(0) {}

        int videoChannel;
        int frameNumber;
    };

    std::atomic<qint64> m_lastMediaTimeUsec; //< UTC usec timestamp for the very last packet

    // Delay video decoding. Used for AV sync.
    std::deque<QnCompressedVideoDataPtr> m_predecodeQueue;

    int m_sequence;

    VideoGeometryAccessor m_videoGeometryAccessor;

    std::atomic<qint64> m_lastFrameTimeUs;
    std::atomic<qint64> m_lastDisplayedTimeUs;
    MultiSensorHelper m_awaitingFramesMask;
    int m_eofPacketCounter;
    std::atomic<bool> m_audioEnabled;
    std::atomic<bool> m_needToResetAudio;
    std::atomic<bool> m_allowOverlay;
    std::atomic<bool> m_allowHardwareAcceleration {false};
    std::atomic<double> m_speed;
    nx::media::StreamEventPacket m_mediaEvent;
    RenderContextSynchronizerPtr m_renderContextSynchronizer;
    const QnArchiveStreamReader* m_archiveReader = nullptr;
};

} // namespace media
} // namespace nx
