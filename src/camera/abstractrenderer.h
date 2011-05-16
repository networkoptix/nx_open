#ifndef clabstract_draw_h_20_57
#define clabstract_draw_h_20_57

#include "../decoders/video/frame_info.h"

class CLAbstractRenderer
{
public:
	CLAbstractRenderer()
        : m_copyData(false)
	{
	}

	virtual void draw(CLVideoDecoderOutput& image, unsigned int channel) = 0;
	virtual void beforeDestroy() = 0;

	virtual QSize sizeOnScreen(unsigned int channel) const = 0;

    virtual bool constantDownscaleFactor() const = 0;

	virtual void copyVideoDataBeforePainting(bool copyData)
	{
		m_copyData = copyData;
	}

protected:
	bool m_copyData;
};

#endif //clgl_draw_h_20_31
