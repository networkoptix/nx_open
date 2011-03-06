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

private:
	QMutex m_mtx;
	CLAbstractVideoDecoder* m_decoder[CL_VARIOUSE_DECODERS];
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
	void allocScaleContext(const CLVideoDecoderOutput& outFrame);
	void freeScaleContext();
};

#endif //videostreamdisplay_h_2044