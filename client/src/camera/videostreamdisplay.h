#ifndef videostreamdisplay_h_2044
#define videostreamdisplay_h_2044

#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QSize>

#include "core/datapacket/mediadatapacket.h"
#include "decoders/videodata.h"

struct AVFrame;
struct SwsContext;

class CLAbstractRenderer;
class CLAbstractVideoDecoder;

/**
  * Display one video stream. Decode the video and pass it to video window.
  */
class CLVideoStreamDisplay
{
public:
    CLVideoStreamDisplay(CLAbstractRenderer *renderer, bool can_downscale);
    ~CLVideoStreamDisplay();

    void dispay(QnCompressedVideoDataPtr data, bool draw,
                CLVideoDecoderOutput::downscale_factor force_factor = CLVideoDecoderOutput::factor_any);

    void copyImage(bool copy);

    CLVideoDecoderOutput::downscale_factor getCurrentDownscaleFactor() const;

private:
    CLAbstractRenderer *const m_renderer;
    bool m_canDownscale;

    QMutex m_mtx;
    QHash<int, CLAbstractVideoDecoder *> m_decoder; // codec Id -> decoder

    /**
      * to reduce image size for weak video cards
      */
    CLVideoDecoderOutput m_outFrame;

    CLVideoDecoderOutput::downscale_factor m_prevFactor;
    CLVideoDecoderOutput::downscale_factor m_scaleFactor;
    QSize m_previousOnScreenSize;

    AVFrame *m_frameYUV;
    quint8* m_buffer;
    SwsContext *m_scaleContext;
    int m_outputWidth;
    int m_outputHeight;

private:
    bool allocScaleContext(const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);
    void freeScaleContext();
    bool rescaleFrame(CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);
    CLVideoDecoderOutput::downscale_factor findScaleFactor(int width, int height, int fitWidth, int fitHeight);
    CLVideoDecoderOutput::downscale_factor determineScaleFactor(QnCompressedVideoDataPtr data, const CLVideoData& img, CLVideoDecoderOutput::downscale_factor force_factor);
};

#endif //videostreamdisplay_h_2044
