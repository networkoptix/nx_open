#include "scene_movement.h"
#include <QGraphicsView>
#include <QScrollBar>
#include "../../base/log.h"

CLSceneMovement::CLSceneMovement(QGraphicsView* gview):
m_view(gview),
m_timeline(CLAnimationTimeLine::CLAnimationCurve::SLOW_END)
{
	m_timeline.setDuration(1000);
	m_timeline.setFrameRange(0, 10000);
	m_timeline.setUpdateInterval(17); // 60 fps

	connect(&m_timeline, SIGNAL(frameChanged(int)), this, SLOT(onNewPos(int)));

	cl_log.log("=========new=========",  cl_logDEBUG1);

}

CLSceneMovement::~CLSceneMovement()
{
	stop();
}

void CLSceneMovement::move (QPointF dest)
{
	stop();


	m_startpoint = m_view->mapToScene(m_view->viewport()->rect().center());
	m_delta = dest - m_startpoint;
	

	m_timeline.start();

}

void CLSceneMovement::stop()
{
	m_timeline.stop();
}

void CLSceneMovement::onNewPos(int pos)
{
	qreal dpos = qreal(pos)/m_timeline.endFrame();

	QPointF move(m_delta.x()*dpos, m_delta.y()*dpos);


	QPointF result = m_startpoint + move;
	m_view->centerOn(result);

	
	//cl_log.log("x=", (int)result.x(), cl_logDEBUG1);

	
}