#include "abstract_animation.h"
#include "../../base/log.h"



CLAbstractAnimation::CLAbstractAnimation():
m_timeline(CLAnimationTimeLine::CLAnimationCurve::SLOW_END)
{
	//m_timeline.setCurveShape(QTimeLine::CurveShape::LinearCurve);
	m_timeline.setDuration(2000);
	m_timeline.setFrameRange(0, 20000);
	m_timeline.setUpdateInterval(17); // 60 fps

	connect(&m_timeline, SIGNAL(frameChanged(int)), this, SLOT(onNewFrame(int)));
}

CLAbstractAnimation::~CLAbstractAnimation()
{
	stop();
}

void CLAbstractAnimation::stop()
{
	m_timeline.stop();
}

