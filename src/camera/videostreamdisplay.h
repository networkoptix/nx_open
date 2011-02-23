#ifndef videostreamdisplay_h_2044
#define videostreamdisplay_h_2044

#include "../decoders/video/frame_info.h"

// display one video stream
// decodes the video and pass it to video window
class CLAbstractVideoDecoder;
struct CLCompressedVideoData;
class CLAbstractRenderer;

class CLVideoStreamDisplay
{
public:
	CLVideoStreamDisplay(bool can_downscale);
	~CLVideoStreamDisplay();
	void setDrawer(CLAbstractRenderer* draw);
	void dispay(CLCompressedVideoData* data, bool draw, CLVideoDecoderOutput::downscale_factor force_factor = CLVideoDecoderOutput::factor_any);

	void setLightCPUMode(bool val);
	void coppyImage(bool copy);

	CLVideoDecoderOutput::downscale_factor getCurrentDownScaleFactor() const;

private:
	QMutex m_mtx;
	CLAbstractVideoDecoder* m_decoder[CL_VARIOUSE_DECODERS];
	CLAbstractRenderer* m_draw;

	CLVideoDecoderOutput m_out_frame; // to reduce image size for weak video cards 
	

	bool m_lightCPUmode;

	bool m_can_downscale;
	

	CLVideoDecoderOutput::downscale_factor m_prevFactor;
	CLVideoDecoderOutput::downscale_factor m_scalefactor;
	QSize m_prev_on_screen_size;

};

#endif //videostreamdisplay_h_2044