#pragma once

#include <deque>
#include <atomic>
#include <functional>

#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/audio_data_packet.h>

#include "media_fwd.h"
#include "audio_output.h"
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
class PlayerDataConsumer:
    public QnAbstractDataConsumer,
    public QnlTimeSource
{
    Q_OBJECT
    typedef QnAbstractDataConsumer base_type;

public:
    typedef std::function<QRect()> VideoGeometryAccessor;

public:
    PlayerDataConsumer(const std::unique_ptr<QnArchiveStreamReader>& archiveReader);
    virtual ~PlayerDataConsumer();

    QVideoFramePtr dequeueVideoFrame();
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

    /** Turn on / off audio. It allowed to call from other thread. */
    void setAudioEnabled(bool value);
    bool isAudioEnabled() const;
signals:
    /** Hint to render to display current data with no delay due to seek operation in progress. */
    void hurryUp();

    /** New video frame is decoded and ready for rendering. */
    void gotVideoFrame();

    /** Jump to new position. */
    void jumpOccurred(int sequence);
private slots:
    void onBeforeJump(qint64 timeUsec);
    void onJumpCanceled(qint64 timeUsec);
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

    void enqueueVideoFrame(QVideoFramePtr decodedFrame);
    int getBufferingMask() const;
    QnCompressedVideoDataPtr queueVideoFrame(const QnCompressedVideoDataPtr& videoFrame);
    bool checkSequence(int sequence);
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

    std::deque<QVideoFramePtr> m_decodedVideo;
    QnWaitCondition m_queueWaitCond;
    mutable QnMutex m_queueMutex; //< sync with player thread
    mutable QnMutex m_jumpMutex; //< sync jump related logic
    mutable QnMutex m_decoderMutex; //< sync with create/destroy decoder

    int m_awaitingJumpCounter; //< how many jump requests are queued

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
    int m_emptyPacketCounter;
    std::atomic<bool> m_audioEnabled;
    std::atomic<bool> m_needResetAudio = false;
};

} // namespace media
} // namespace nx
