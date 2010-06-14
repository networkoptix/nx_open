#include "graphicsview.h"
#include <QWheelEvent>
#include <QScrollBar>
#include "./base/log.h"
#include <math.h>
#include "../src/gui/graphicsview/qgraphicsitem.h"
#include "uvideo_wnd.h"
#include "videodisplay/video_cam_layout/videocamlayout.h"
#include "camera/camera.h"


int item_select_duration = 800;

//==============================================================================

GraphicsView::GraphicsView():
QGraphicsView(),
m_xRotate(0), m_yRotate(0),
m_movement(this),
m_scenezoom(this),
m_handScrolling(false),
m_handMoving(0),
m_selectedWnd(0),
m_camLayout(0)
{

}

GraphicsView::~GraphicsView()
{
	
}

void GraphicsView::setCamLayOut(const VideoCamerasLayout* lo)
{
	m_camLayout = lo;
}

void GraphicsView::setRealSceneRect(QRect rect)
{
	m_realSceneRect = rect;
}

QRect GraphicsView::getRealSceneRect() const
{
	return m_realSceneRect;
}


void GraphicsView::wheelEvent ( QWheelEvent * e )
{
	m_scenezoom.restoreDefaultDuration();
	int numDegrees = e->delta() ;
	m_scenezoom.zoom_delta(numDegrees/3000.0);

	if (m_scenezoom.getZoom()<0.285 && numDegrees<0)
	{
		if (m_selectedWnd)
			m_selectedWnd->setSelected(false);

		m_selectedWnd  = 0;
	}
}

void GraphicsView::zoomDefault()
{
	m_scenezoom.zoom_default();
}

void GraphicsView::updateTransform()
{
	QTransform tr;


	QPoint center = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());

	tr.translate(center.x(), center.y());
	tr.rotate(m_yRotate/1.0, Qt::ZAxis);
	tr.rotate(m_xRotate/1.0, Qt::XAxis);
	tr.scale(0.05, 0.05);
	tr.translate(-center.x(), -center.y());

	setTransform(tr);

}


void GraphicsView::mousePressEvent ( QMouseEvent * event)
{
	m_scenezoom.stop();
	m_movement.stop();

	if (event->button() == Qt::LeftButton) 
	{

		// Left-button press in scroll hand mode initiates hand scrolling.


		m_mousestate.clearEvents();
		m_mousestate.mouseMoveEventHandler(event);
        event->accept();
        m_handScrolling = true;
		viewport()->setCursor(Qt::ClosedHandCursor);
	}
}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
	if (m_handScrolling) 
	{
		QPoint delta = event->pos() - m_mousestate.getLastEventPoint();


		QScrollBar *hBar = horizontalScrollBar();
		QScrollBar *vBar = verticalScrollBar();
		hBar->setValue(hBar->value() + (isRightToLeft() ? delta.x() : -delta.x()));
		vBar->setValue(vBar->value() - delta.y());
		
		//cl_log.log("==m_handMoving!!!=====", cl_logDEBUG1);

		++m_handMoving;

		m_mousestate.mouseMoveEventHandler(event);

		
	}	
	QGraphicsView::mouseMoveEvent(event);
}

void GraphicsView::mouseReleaseEvent ( QMouseEvent * event)
{
	

	//cl_log.log("====mouseReleaseEvent===", cl_logDEBUG1);

	bool left_button = event->button() == Qt::LeftButton;
	bool right_button = event->button() == Qt::RightButton;
	bool handMoving = m_handMoving>2;

	m_scenezoom.restoreDefaultDuration();
	m_movement.restoreDefaultDuration();

	if (left_button) // if left button released
	{
		viewport()->setCursor(Qt::OpenHandCursor);
		m_handScrolling = false;
	}


	if (handMoving && left_button) // if scene moved and left button released
	{
		// need to continue movement(animation)

		m_mousestate.mouseMoveEventHandler(event);

		qreal mouse_spped, mouse_spped_h, mouse_spped_v;

		m_mousestate.getMouseSpeed(mouse_spped, mouse_spped_h, mouse_spped_v);


		if (mouse_spped>150)
		{
			bool sdx = (mouse_spped_h<0);
			bool sdy = (mouse_spped_v<0);

			int dx = pow( abs(mouse_spped_h), 2.0)/1900;
			int dy = pow( abs(mouse_spped_v), 2.0)/1900;;

			if (sdx) dx =-dx;
			if (sdy) dy =-dy;

			m_movement.setDuration(m_movement.getDefaultDuration() + mouse_spped);
			m_movement.move(-dx,-dy);
		}
	}


	if (!handMoving) // if left button released and we did not move the scene, so may bee need to zoom on the item
	{

			QGraphicsItem *item = itemAt(event->pos());
			VideoWindow* wnd = static_cast<VideoWindow*>(item);

			if(!wnd) // not item and any button
			{

				// zero item selected 
				if (m_movement.isRuning() || m_scenezoom.isRuning())
				{
					// if something moves => stop moving 
					m_movement.stop();
					m_scenezoom.stop();
				}

				else
				{
					// move to the new point

					//if (right_button)
					{
						if (m_selectedWnd)
							m_movement.setDuration(item_select_duration);

						m_movement.move(mapToScene(event->pos()));
					}

					if (m_selectedWnd)
						m_scenezoom.setDuration(item_select_duration);
					m_scenezoom.zoom_default();
				}

				// selection should be zerro anyway
				if (m_selectedWnd) 
					m_selectedWnd->setSelected(false);
				m_selectedWnd = 0;
			}
			

			if (wnd && wnd!=m_selectedWnd && left_button ) // item and left button
			{
				// new item selected 
				onNewItemSelected_helper(wnd);
			}

	}

	if (left_button)
		m_handMoving = 0;
}


void GraphicsView::keyPressEvent( QKeyEvent * e )
{
	
	switch (e->key()) 
	{
		case Qt::Key_W:
			m_yRotate -= 1;
			updateTransform();
			break;
	}


	if (m_selectedWnd)
	{
		
		CLVideoCamera* cam = m_selectedWnd->getVideoCam();
		CLVideoCamera* next_cam = 0;

		switch (e->key()) 
		{
		case Qt::Key_Left:
			next_cam = m_camLayout->getNextLeftCam(cam);
			break;
		case Qt::Key_Right:
			next_cam = m_camLayout->getNextRightCam(cam);
			break;
		case Qt::Key_Up:
			next_cam = m_camLayout->getNextTopCam(cam);
			break;
		case Qt::Key_Down:
			next_cam = m_camLayout->getNextBottomCam(cam);
			break;
		}

		if (next_cam)
		{
			CLVideoWindow* wnd = (next_cam->getVideoWindow());

			if (wnd)
				onNewItemSelected_helper(static_cast<VideoWindow*>(wnd));
		}
		

	}


}

//=====================================================

void GraphicsView::onNewItemSelected_helper(VideoWindow* new_wnd)
{
	if (m_selectedWnd) 
		m_selectedWnd->setSelected(false);

	m_selectedWnd = new_wnd;


	QPointF point = m_selectedWnd->mapToScene(m_selectedWnd->boundingRect().center());


	m_movement.setDuration(item_select_duration);
	m_movement.move(point);

	m_scenezoom.setDuration(item_select_duration);
	m_scenezoom.zoom_abs(0.30);


	m_selectedWnd->setSelected(true);

}