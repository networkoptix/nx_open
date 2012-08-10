#ifndef QN_VIDEO_STREAM_DISPLAY_H
#define QN_VIDEO_STREAM_DISPLAY_H


#include "decoders/video/abstractdecoder.h"
#include "decoders/frame_scaler.h"

class QnAbstractVideoDecoder;
struct QnCompressedVideoData;
class QnAbstractRenderer;
class QnBufferedFrameDisplayer;

static const int MAX_FRAME_QUEUE_SIZE = 12;
static const int MAX_QUEUE_TIME = 1000 * 200;


/**
  * Display one video stream. Decode the video and pass it to video window.
  */
class QnVideoStreamDisplay
{
public:
    enum FrameDisplayStatus {Status_Displayed, Status_Skipped, Status_Buffered};

    QnVideoStreamDisplay(bool can_downscale);
    ~QnVideoStreamDisplay();
    void setDrawer(QnAbstractRenderer* draw);
    FrameDisplayStatus dispay(QnCompressedVideoDataPtr data, bool draw,
                QnFrameScaler::DownscaleFactor force_factor = QnFrameScaler::factor_any);

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    QnFrameScaler::DownscaleFactor getCurrentDownscaleFactor() const;

    void setMTDecoding(bool value);

    void setSpeed(float value);
    qint64 getLastDisplayedTime() const;
    void setLastDisplayedTime(qint64 value);
    void afterJump();
    QSize getFrameSize() const;
    QImage getScreenshot();
    void blockTimeValue(qint64 time);
    void unblockTimeValue();
    bool isTimeBlocked() const;
    void setCurrentTime(qint64 time);
    void waitForFramesDisplaed();
    void onNoVideo();
    void canUseBufferedFrameDisplayer(bool value);
    qint64 nextReverseTime() const;
    QSize getImageSize() const;

    /**
      * Return last decoded frame
      */
    QSharedPointer<CLVideoDecoderOutput> flush(QnFrameScaler::DownscaleFactor force_factor, int channelNum);

private:
    mutable QMutex m_mtx;
    mutable QMutex m_timeMutex;
    QMap<CodecID, QnAbstractVideoDecoder*> m_decoder;

    QnAbstractRenderer* m_drawer;

    /**
      * to reduce image size for weak video cards 
      */

    CLVideoDecoderOutput* m_frameQueue[MAX_FRAME_QUEUE_SIZE];
    CLVideoDecoderOutput* m_prevFrameToDelete;
    int m_frameQueueIndex;

    QnAbstractVideoDecoder::DecodeMode m_decodeMode;
    bool m_canDownscale;

    QnFrameScaler::DownscaleFactor m_prevFactor;
    QnFrameScaler::DownscaleFactor m_scaleFactor;
    QSize m_previousOnScreenSize;

    SwsContext *m_scaleContext;
    int m_outputWidth;
    int m_outputHeight;
    bool m_enableFrameQueue;
    bool m_queueUsed;
    bool m_needReinitDecoders;
    bool m_reverseMode;
    bool m_prevReverseMode;
    QQueue<CLVideoDecoderOutput*> m_reverseQueue;
    bool m_flushedBeforeReverseStart;
    qint64 m_lastDisplayedTime;
    int m_realReverseSize;
    int m_maxReverseQueueSize;
    bool m_timeChangeEnabled;
    QnBufferedFrameDisplayer* m_bufferedFrameDisplayer;
    bool m_canUseBufferedFrameDisplayer;
private:
    float m_speed;
    bool m_queueWasFilled;
    bool m_needResetDecoder;
    CLVideoDecoderOutput* m_lastDisplayedFrame;
    QSize m_imageSize;
    int m_prevSrcWidth;
    int m_prevSrcHeight;

    void reorderPrevFrames();
    bool allocScaleContext(const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);
    void freeScaleContext();
    bool rescaleFrame(const CLVideoDecoderOutput& srcFrame, CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);

    QnFrameScaler::DownscaleFactor findScaleFactor(int width, int height, int fitWidth, int fitHeight);
    QnFrameScaler::DownscaleFactor determineScaleFactor(
        int channelNumber, 
        int srcWidth, 
        int srcHeight, 
        QnFrameScaler::DownscaleFactor force_factor);
    bool processDecodedFrame(QnAbstractVideoDecoder* dec, CLVideoDecoderOutput* outFrame, bool enableFrameQueue, bool reverseMode);
    void checkQueueOverflow(QnAbstractVideoDecoder* dec);
    void clearReverseQueue();
};

#endif //QN_VIDEO_STREAM_DISPLAY_H
