#include "abstract_animation.h"
#include "../../base/log.h"



CLAbstractAnimation::CLAbstractAnimation():
m_timeline(CLAnimationTimeLine::CLAnimationCurve::SLOW_END_POW_40)
{
	//m_timeline.setCurveShape(QTimeLine::CurveShape::LinearCurve);
	m_timeline.setDuration(2000);
	m_timeline.setFrameRange(0, 200000);
	m_timeline.setUpdateInterval(5); // 60 fps


	connect(&m_timeline, SIGNAL(valueChanged(qreal)), this, SLOT(valueChanged(qreal)));
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

void CLAbstractAnimation::start_helper(int duration)
{
	m_timeline.setDuration(duration);

	if (!isRuning())
	{
		m_timeline.start();
		m_timeline.start();
	}
	m_timeline.setCurrentTime(0);
}

void CLAbstractAnimation::onFinished()
{
	//restoreDefaultDuration();
}

