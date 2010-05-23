#ifndef clabstract_draw_h_20_57
#define clabstract_draw_h_20_57

#include "../decoders/video/frame_info.h"
#include <QMutex>
//#include <QWaitCondition>

class CLAbstractRenderer
{
public:
	CLAbstractRenderer():
	m_copydata(false),
	m_aspectratio(4.0f/3)
	{

	}

	virtual void draw(CLVideoDecoderOutput& image, unsigned int channel)=0;
	virtual void before_destroy() = 0;

	virtual void copyVideoDataBeforePainting(bool copy)
	{
		m_copydata = copy;
	}

	virtual float aspectRatio() const
	{
		QMutexLocker  locker(&m_mutex_aspect);	
		return m_aspectratio;
	}

protected:
	bool m_copydata;

	mutable QMutex m_mutex_aspect;
	float m_aspectratio;

};


#endif //clgl_draw_h_20_31