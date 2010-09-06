#include "scene_movement.h"
#include <QScrollBar>
#include "../../base/log.h"
#include "../graphicsview.h"
#include "../videoitem/uvideo_wnd.h"



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

	return val;
}

//========================================================================================

CLSceneMovement::CLSceneMovement(GraphicsView* gview):
m_view(gview),
m_limited(false)
{
	
}

CLSceneMovement::~CLSceneMovement()
{
	stop();
}


void CLSceneMovement::move(int dx, int dy, int duration, bool limited)
{
	QPoint curr = m_view->viewport()->rect().center();
	curr.rx()+=dx;
	curr.ry()+=dy;

	move(m_view->mapToScene(curr), duration);

	m_limited = limited; // this will overwrite false value set inside move_abs function above
}

void CLSceneMovement::move (QPointF dest, int duration)
{
	//cl_log.log("CLSceneMovement::move() ", cl_logDEBUG1);

	m_limited = false;

	m_startpoint = m_view->mapToScene(m_view->viewport()->rect().center());
	m_delta = dest - m_startpoint;
	
	start_helper(duration);
}

void CLSceneMovement::valueChanged ( qreal dpos )
{
	QPointF move(m_delta.x()*dpos, m_delta.y()*dpos);


	QPointF result = m_startpoint + move;

	
	QRect rsr = m_view->getRealSceneRect();

	result.rx() = limit_val(result.x(), rsr.left(), rsr.right(), true);
	result.ry() = limit_val(result.y(), rsr.top(), rsr.bottom(), true);
	
	
	m_view->centerOn(result);

	/*
	QTransform tr;
	tr.translate(result.x(), result.y());
	tr.rotate(-1.0, Qt::YAxis);
	tr.translate(-result.x(), -result.y());
	m_view->setTransform(tr);
	/**/


	//=======================================
	if (m_limited && m_view->getSelectedWnd())
	{
		VideoWindow* wnd = m_view->getSelectedWnd();

		QPointF wnd_center = wnd->mapToScene(wnd->boundingRect().center());
		QPointF final_dest = m_startpoint + m_delta;
		QPointF curr = m_view->mapToScene(m_view->viewport()->rect().center());

		if (QLineF(final_dest, wnd_center).length() > QLineF(curr, wnd_center).length()   ) 
		{
			// if difference between final point and wnd center is more than curr and wnd center => moving out of wnd center
			int dx = curr.x() - wnd_center.x();
			int dy = curr.y() - wnd_center.y();

			qreal percent = 0.05;

			QRectF wnd_rect =  wnd->sceneBoundingRect();
			QRectF viewport_rec = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();


			if (dx<0)
			{
				if ( (wnd_rect.topLeft().x() -  viewport_rec.topLeft().x()) >  percent*wnd_rect.width())
					stop();
			}

			if (dx>0)
			{
				if ( (viewport_rec.topRight().x() - wnd_rect.topRight().x()) >  percent*wnd_rect.width())
					stop();
			}

			if (dy<0)
			{
				if ( (wnd_rect.topLeft().y() - viewport_rec.topLeft().y()) >  percent*wnd_rect.height())
					stop();
			}

			if (dy>0)
			{
				if ( (viewport_rec.bottomLeft().y()- wnd_rect.bottomRight().y()) >  percent*wnd_rect.height())
					stop();
			}
			
			
		}


	}
	else if (m_view->getSelectedWnd())
	{
		VideoWindow* wnd = m_view->getSelectedWnd();

		QPointF selected_center = wnd->mapToScene(wnd->boundingRect().center());
		QPointF dest = m_startpoint + m_delta;
		

		QPointF diff = selected_center - dest;

		if ( abs(diff.x())< 1.0 &&  abs(diff.y())< 1.0 )
			return; // moving to the selection => do not need to unselect

		// else
		diff = selected_center - result;


		if ( abs(diff.x()) > wnd->boundingRect().width()*1.2 || abs(diff.y()) > wnd->boundingRect().height()*1.2)
			m_view->setZeroSelection();
	}

	
}