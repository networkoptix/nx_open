#ifndef videostreamdisplay_h_2044
#define videostreamdisplay_h_2044


#include "decoders/video/abstractdecoder.h"

class QnAbstractVideoDecoder;
struct QnCompressedVideoData;
class CLAbstractRenderer;
class BufferedFrameDisplayer;

static const int MAX_FRAME_QUEUE_SIZE = 12;

/**
  * Display one video stream. Decode the video and pass it to video window.
  */
class CLVideoStreamDisplay
{
public:
    enum FrameDisplayStatus {Status_Displayed, Status_Skipped, Status_Buffered};

    CLVideoStreamDisplay(bool can_downscale);
    ~CLVideoStreamDisplay();
    void setDrawer(CLAbstractRenderer* draw);
    FrameDisplayStatus dispay(QnCompressedVideoDataPtr data, bool draw,
                CLVideoDecoderOutput::downscale_factor force_factor = CLVideoDecoderOutput::factor_any);

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    CLVideoDecoderOutput::downscale_factor getCurrentDownscaleFactor() const;

    void setMTDecoding(bool value);

    void setSpeed(float value);
    qint64 getLastDisplayedTime() const;
    void setLastDisplayedTime(qint64 value);
    void afterJump();
    QImage getScreenshot();
    void blockTimeValue(qint64 time);
    void unblockTimeValue();
    bool isTimeBlocked() const;
    void setCurrentTime(qint64 time);
    void waitForFramesDisplaed();
    void onNoVideo();
    void canUseBufferedFrameDisplayer(bool value);
private:
    QMutex m_mtx;
    mutable QMutex m_timeMutex;
    QMap<CodecID, QnAbstractVideoDecoder*> m_decoder;

    CLAbstractRenderer* m_drawer;

    /**
      * to reduce image size for weak video cards 
      */

    CLVideoDecoderOutput* m_frameQueue[MAX_FRAME_QUEUE_SIZE];
    CLVideoDecoderOutput* m_prevFrameToDelete;
    int m_frameQueueIndex;

    QnAbstractVideoDecoder::DecodeMode m_lightCPUmode;
    bool m_canDownscale;

    CLVideoDecoderOutput::downscale_factor m_prevFactor;
    CLVideoDecoderOutput::downscale_factor m_scaleFactor;
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
    BufferedFrameDisplayer* m_bufferedFrameDisplayer;
    bool m_canUseBufferedFrameDisplayer;
private:
    bool m_queueWasFilled;
    float m_speed;
    void reorderPrevFrames();
    bool allocScaleContext(const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);
    void freeScaleContext();
    bool rescaleFrame(const CLVideoDecoderOutput& srcFrame, CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);

    CLVideoDecoderOutput::downscale_factor findScaleFactor(int width, int height, int fitWidth, int fitHeight);
    CLVideoDecoderOutput::downscale_factor determineScaleFactor(
        QnCompressedVideoDataPtr data, 
        int srcWidth, 
        int srcHeight, 
        CLVideoDecoderOutput::downscale_factor force_factor);
    bool processDecodedFrame(QnAbstractVideoDecoder* dec, CLVideoDecoderOutput* outFrame, bool enableFrameQueue, bool reverseMode);
    void checkQueueOverflow(QnAbstractVideoDecoder* dec);
    void clearReverseQueue();
};

#endif //videostreamdisplay_h_2044
