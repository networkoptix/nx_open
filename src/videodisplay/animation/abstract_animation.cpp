#include "abstract_animation.h"
#include "../../base/log.h"



CLAbstractAnimation::CLAbstractAnimation():
m_timeline(CLAnimationTimeLine::CLAnimationCurve::SLOW_END_POW_40)
{
	//m_timeline.setCurveShape(QTimeLine::CurveShape::LinearCurve);
	m_timeline.setDuration(2000);
	m_timeline.setFrameRange(0, 200000);
	m_timeline.setUpdateInterval(5); // 60 fps

	setDefaultDuration(2000);

	connect(&m_timeline, SIGNAL(frameChanged(int)), this, SLOT(onNewFrame(int)));
	connect(&m_timeline, SIGNAL(finished()), this, SLOT(onFinished()));
}


CLAbstractAnimation::~CLAbstractAnimation()
{
	stop();
}

bool CLAbstractAnimation::isRuning() const
{
	return (m_timeline.state() & QTimeLine::Running);
}

void CLAbstractAnimation::stop()
{
	m_timeline.stop();
}

void CLAbstractAnimation::setDuration(int ms)
{
	m_timeline.setDuration(ms);
}

void CLAbstractAnimation::onFinished()
{
	//restoreDefaultDuration();
}

void CLAbstractAnimation::setDefaultDuration(int val)
{
	m_defaultduration = val;
}

void CLAbstractAnimation::restoreDefaultDuration()
{
	m_timeline.setDuration(m_defaultduration );
}

int CLAbstractAnimation::getDefaultDuration() const
{
	return m_defaultduration;
}