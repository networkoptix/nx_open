#include "scene_movement.h"
#include <QScrollBar>
#include "../../base/log.h"
#include "graphicsview.h"


int limit_val(int val, int min_val, int max_val, bool mirror)
{

	if (!mirror)
	{
		if (val > max_val)
			return max_val;

		if (val < min_val)
			return min_val;
	}


	int width = max_val - min_val;

	if (val > max_val)
	{
		int x = val - min_val;

		x = (x%(2*width));// now x cannot be more than 2*width
		x+=min_val;

		if (x > max_val)
		{
			int diff = x - max_val; // so many bigger 
			x = max_val - diff; // now x cannot be more than 2*width
		}

		return x;
	}

	if (val < min_val)
	{

		int x = max_val - val;
		x = x%(2*width);
		x = max_val - x;

		if (x < min_val)
		{
			int diff = min_val - x; // so many smaller 
			x = min_val + diff; 
		}

		return x;

	}

}

//========================================================================================

CLSceneMovement::CLSceneMovement(GraphicsView* gview):
m_view(gview)
{
	

	//m_timeline.setCurve(CLAnimationTimeLine::CLAnimationCurve::SLOW_END_POW_35);

	const int dur = 3000;

	m_timeline.setDuration(dur);
	setDefaultDuration(dur);

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

	
	QRect rsr = m_view->getRealSceneRect();


	result.rx() = limit_val(result.x(), rsr.left(), rsr.right(), true);
	result.ry() = limit_val(result.y(), rsr.top(), rsr.bottom(), true);
	

	
	m_view->centerOn(result);

	
}