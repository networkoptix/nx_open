#ifndef clabstract_draw_h_20_57
#define clabstract_draw_h_20_57

#include "../decoders/video/frame_info.h"

class CLAbstractRenderer
{
public:
	CLAbstractRenderer():
	m_copydata(false)
	{

	}

	virtual void draw(CLVideoDecoderOutput& image)=0;
	virtual void before_destroy() = 0;
	virtual float aspectRatio() const = 0;

	virtual void copyVideoDataBeforePainting(bool copy)
	{
		m_copydata = copy;
	}
protected:
	bool m_copydata;
};


#endif //clgl_draw_h_20_31