#include "abstract_animation.h"
#include "../../base/log.h"
#include "../src/corelib/kernel/qtimer.h"



CLAbstractAnimation::CLAbstractAnimation():
m_timeline(CLAnimationTimeLine::CLAnimationCurve::SLOW_END_POW_40)
{
	//m_timeline.setCurveShape(QTimeLine::CurveShape::LinearCurve);
	m_timeline.setDuration(2000);
	m_timeline.setFrameRange(0, 200000);
	m_timeline.setUpdateInterval(1000/200); // 60 fps

	m_delay_timer.setSingleShot(true);


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
	m_delay_timer.stop();
}

void CLAbstractAnimation::Start()
{
	m_timeline.start();
}

void CLAbstractAnimation::start_helper(int duration, int delay)
{
	m_timeline.setDuration(duration);

	if (!isRuning())
	{

		m_delay_timer.stop(); // just in case if it's still running

		if (delay==0)
			m_timeline.start();
		else
		{
			//QTimer::singleShot(delay, this, SLOT(Start()));
			connect(&m_delay_timer, SIGNAL(timeout()), this, SLOT(Start()));
			m_delay_timer.start(delay);

		}
	}
	m_timeline.setCurrentTime(0);
}

void CLAbstractAnimation::onFinished()
{
	emit finished();
}

