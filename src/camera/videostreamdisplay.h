#ifndef videostreamdisplay_h_2044
#define videostreamdisplay_h_2044

#include "../decoders/video/frame_info.h"
#include "decoders/video/abstractdecoder.h"

class CLAbstractVideoDecoder;
struct CLCompressedVideoData;
class CLAbstractRenderer;

static const int MAX_FRAME_QUEUE_SIZE = 2;

/**
  * Display one video stream. Decode the video and pass it to video window.
  */
class CLVideoStreamDisplay
{
public:
    CLVideoStreamDisplay(bool can_downscale);
    ~CLVideoStreamDisplay();
    void setDrawer(CLAbstractRenderer* draw);
    bool dispay(CLCompressedVideoData* data, bool draw,
                CLVideoDecoderOutput::downscale_factor force_factor = CLVideoDecoderOutput::factor_any);

    void setLightCPUMode(CLAbstractVideoDecoder::DecodeMode val);

    CLVideoDecoderOutput::downscale_factor getCurrentDownscaleFactor() const;

    void setMTDecoding(bool value);

    void setReverseMode(bool value);

    qint64 getLastDisplayedTime() const;
    void setLastDisplayedTime(qint64 value);

private:
    QMutex m_mtx;
    mutable QMutex m_timeMutex;
    QMap<CodecID, CLAbstractVideoDecoder*> m_decoder;

    CLAbstractRenderer* m_drawer;

    /**
      * to reduce image size for weak video cards 
      */

    CLVideoDecoderOutput* m_frameQueue[MAX_FRAME_QUEUE_SIZE];
    CLVideoDecoderOutput* m_prevFrameToDelete;
    int m_frameQueueIndex;

    CLAbstractVideoDecoder::DecodeMode m_lightCPUmode;
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
    QQueue<CLVideoDecoderOutput*> m_reverseQueue;
    bool m_flushedBeforeReverseStart;
    qint64 m_lastDisplayedTime;
    int m_realReverseSize;
    int m_maxReverseQueueSize;
private:
    void reorderPrevFrames();
    bool allocScaleContext(const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);
    void freeScaleContext();
    bool rescaleFrame(const CLVideoDecoderOutput& srcFrame, CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);

    CLVideoDecoderOutput::downscale_factor findScaleFactor(int width, int height, int fitWidth, int fitHeight);
    CLVideoDecoderOutput::downscale_factor determineScaleFactor(
        CLCompressedVideoData* data, 
        int srcWidth, 
        int srcHeight, 
        CLVideoDecoderOutput::downscale_factor force_factor);
    void processDecodedFrame(int channel, CLVideoDecoderOutput* outFrame, bool enableFrameQueue, bool reverseMode);
    void checkQueueOverflow(CLAbstractVideoDecoder* dec);
};

#endif //videostreamdisplay_h_2044
