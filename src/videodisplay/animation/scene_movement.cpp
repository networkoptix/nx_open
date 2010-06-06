#include "scene_movement.h"
#include <QGraphicsView>
#include <QScrollBar>
#include "../../base/log.h"

CLSceneMovement::CLSceneMovement(QGraphicsView* gview):
m_view(gview)
{
	

	//m_timeline.setCurve(CLAnimationTimeLine::CLAnimationCurve::SLOW_END_POW_40);

	const int dur = 3000;

	m_timeline.setDuration(dur);
	setDefaultDuration(dur);
	m_timeline.setFrameRange(0, 20000);
}

CLSceneMovement::~CLSceneMovement()
{
	stop();
}


void CLSceneMovement::move (int dx, int dy)
{
	QPoint curr = m_view->viewport()->rect().center();
	curr.rx()+=dx;
	curr.ry()+=dy;


	move(m_view->mapToScene(curr));
}

void CLSceneMovement::move (QPointF dest)
{
	//cl_log.log("CLSceneMovement::move() ", cl_logDEBUG1);

	m_startpoint = m_view->mapToScene(m_view->viewport()->rect().center());
	m_delta = dest - m_startpoint;
	

	if (!isRuning())
	{
		m_timeline.start();
		m_timeline.start();
	}

	
	m_timeline.setCurrentTime(0);

}


void CLSceneMovement::onNewFrame(int pos)
{
	qreal dpos = qreal(pos)/m_timeline.endFrame();

	QPointF move(m_delta.x()*dpos, m_delta.y()*dpos);


	QPointF result = m_startpoint + move;
	m_view->centerOn(result);

	
}