#ifndef videostreamdisplay_h_2044
#define videostreamdisplay_h_2044

#include "../decoders/video/frame_info.h"

class CLAbstractVideoDecoder;
struct CLCompressedVideoData;
class CLAbstractRenderer;

/**
  * Display one video stream. Decode the video and pass it to video window.
  */
class CLVideoStreamDisplay
{
public:
	CLVideoStreamDisplay(bool can_downscale);
	~CLVideoStreamDisplay();
	void setDrawer(CLAbstractRenderer* draw);
	void dispay(CLCompressedVideoData* data, bool draw, CLVideoDecoderOutput::downscale_factor force_factor = CLVideoDecoderOutput::factor_any);

	void setLightCPUMode(bool val);
	void copyImage(bool copy);

	CLVideoDecoderOutput::downscale_factor getCurrentDownscaleFactor() const;

    void setMTDecoding(bool value);
private:
	QMutex m_mtx;
    QMap<CodecID, CLAbstractVideoDecoder*> m_decoder;

	CLAbstractRenderer* m_draw;

    /**
      * to reduce image size for weak video cards 
      */
	CLVideoDecoderOutput m_outFrame;

	bool m_lightCPUmode;
	bool m_canDownscale;

	CLVideoDecoderOutput::downscale_factor m_prevFactor;
	CLVideoDecoderOutput::downscale_factor m_scaleFactor;
	QSize m_previousOnScreenSize;

	AVFrame *m_frameYUV;
	uint8_t* m_buffer;
	SwsContext *m_scaleContext;
	int m_outputWidth;
	int m_outputHeight;

private:
	bool allocScaleContext(const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);
	void freeScaleContext();
    bool rescaleFrame(CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);
    CLVideoDecoderOutput::downscale_factor findScaleFactor(int width, int height, int fitWidth, int fitHeight);
    CLVideoDecoderOutput::downscale_factor findOversizeScaleFactor(int width, int height, int fitWidth, int fitHeight);
};

#endif //videostreamdisplay_h_2044
