#ifndef clabstract_draw_h_20_57
#define clabstract_draw_h_20_57

#include "../decoders/video/frame_info.h"

class CLAbstractRenderer
{
public:
	CLAbstractRenderer()
	{
	}

	virtual void draw(CLVideoDecoderOutput* image) = 0;
    virtual void waitForFrameDisplayed(int channel) = 0;
	virtual void beforeDestroy() = 0;

	virtual QSize sizeOnScreen(unsigned int channel) const = 0;

    virtual bool constantDownscaleFactor() const = 0;

    QMutex& getMutex() { return m_displaySync; }
protected:
    mutable QMutex m_displaySync; // to avoid call paintEvent() more than once at the same time
    QWaitCondition m_waitCon;
};

#endif //clgl_draw_h_20_31
