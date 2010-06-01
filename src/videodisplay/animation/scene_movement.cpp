#include "scene_movement.h"
#include <QGraphicsView>
#include <QScrollBar>
#include "../../base/log.h"

CLSceneMovement::CLSceneMovement(QGraphicsView* gview):
m_view(gview)
{
	m_timeline.setDuration(1000);
	m_timeline.setFrameRange(0, 10000);
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

	//cl_log.log("==============", cl_logDEBUG1);
	m_timeline.setCurrentTime(0);

}


void CLSceneMovement::onNewFrame(int pos)
{
	qreal dpos = qreal(pos)/m_timeline.endFrame();

	QPointF move(m_delta.x()*dpos, m_delta.y()*dpos);


	QPointF result = m_startpoint + move;
	m_view->centerOn(result);

	
	//cl_log.log("x=", (int)pos, cl_logDEBUG1);
	
}