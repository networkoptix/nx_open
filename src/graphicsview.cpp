#include "graphicsview.h"
#include <QWheelEvent>
#include <QScrollBar>
#include "./base/log.h"
#include <math.h>
#include "../src/gui/graphicsview/qgraphicsitem.h"
#include "uvideo_wnd.h"
#include "videodisplay/video_cam_layout/videocamlayout.h"
#include "camera/camera.h"



int item_select_duration = 700;
int item_hoverevent_duration = 300;
int scene_zoom_duration = 2500;
int scene_move_duration = 3000;

int item_rotation_duration = 2000;

qreal selected_item_zoom = 1.63;

//==============================================================================

GraphicsView::GraphicsView():
QGraphicsView(),
m_xRotate(0), m_yRotate(0),
m_movement(this),
m_scenezoom(this),
m_handScrolling(false),
m_handMoving(0),
m_rotationCounter(0),
m_selectedWnd(0),
m_last_selectedWnd(0),
m_camLayout(0),
m_ignore_release_event(false),
m_rotatingWnd(0)
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

VideoWindow* GraphicsView::getSelectedWnd() const
{
	return m_selectedWnd;
}

qreal GraphicsView::getZoom() const
{
	return m_scenezoom.getZoom();
}

void GraphicsView::setZeroSelection()
{
	if (m_selectedWnd)
		m_selectedWnd->setSelected(false);

	m_last_selectedWnd = m_selectedWnd;
	m_selectedWnd  = 0;
}

void GraphicsView::setAllItemsQuality(CLStreamreader::StreamQuality q, bool increase)
{
	cl_log.log("new quality", q, cl_logDEBUG1);
	
	QSet<CLVideoCamera*> camlst = m_camLayout->getCamList();

	QSetIterator<CLVideoCamera*> it(camlst);
	while (it.hasNext())
	{
		CLVideoCamera* cam = it.next();

		cam->setQuality(q, increase);
	}
	
}

//================================================================

void GraphicsView::wheelEvent ( QWheelEvent * e )
{
	int numDegrees = e->delta() ;
	m_scenezoom.zoom_delta(numDegrees/3000.0, scene_zoom_duration);
}

void GraphicsView::zoomDefault(int duration)
{
	m_scenezoom.zoom_default(duration);
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
	
	QGraphicsItem *item = itemAt(event->pos());
	VideoWindow* wnd = static_cast<VideoWindow*>(item);

	if (wnd)
		wnd->stop_animation();


	if (event->button() == Qt::LeftButton) 
	{
		// Left-button press in scroll hand mode initiates hand scrolling.

        m_handScrolling = true;
		viewport()->setCursor(Qt::ClosedHandCursor);
	}
	else if (event->button() == Qt::RightButton) 
	{
		m_rotatingWnd = wnd;

	}

	m_mousestate.clearEvents();
	m_mousestate.mouseMoveEventHandler(event);


}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
	bool left_button = event->buttons() & Qt::LeftButton;
	bool right_button = event->buttons() & Qt::RightButton;

	if (m_handScrolling && left_button) 
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

	if (m_rotatingWnd && right_button)
	{

		if (isWndStillExists(m_rotatingWnd))
		{

			/*
			center---------old
			|
			|
			|
			new

			*/

			++m_rotationCounter;

			if (m_rotationCounter>1)
			{
				QPointF center_point = m_rotatingWnd->boundingRect().center(); // by default center is the center of the item

				//if (wnd->isFullScreen()) // if wnd is in full scree mode center must be changed to the cetre of the viewport
				//	center_point = item->mapFromScene(mapToScene(viewport()->rect().center()));

				QPointF old_point = m_rotatingWnd->mapFromScene(mapToScene(m_mousestate.getLastEventPoint()));
				QPointF new_point = m_rotatingWnd->mapFromScene(mapToScene(event->pos()));

				QLineF old_line(center_point, old_point);
				QLineF new_line(center_point, new_point);

				m_rotatingWnd->setRotationPoint(center_point, new_point);
				m_rotatingWnd->drawRotationHelper(true);



				qreal angle = new_line.angleTo(old_line);

				m_rotatingWnd->z_rotate_delta(center_point, angle, 0);
			}
			
		}

		
		
		m_mousestate.mouseMoveEventHandler(event);
	}


	QGraphicsView::mouseMoveEvent(event);
}

void GraphicsView::mouseReleaseEvent ( QMouseEvent * event)
{
	if (m_ignore_release_event)
	{
		m_ignore_release_event = false;
		return;
	}

	//cl_log.log("====mouseReleaseEvent===", cl_logDEBUG1);

	bool left_button = event->button() == Qt::LeftButton;
	bool right_button = event->button() == Qt::RightButton;
	bool handMoving = m_handMoving>2;
	bool rotating = m_rotationCounter>2;


	if (left_button) // if left button released
	{
		viewport()->setCursor(Qt::OpenHandCursor);
		m_handScrolling = false;
	}

	//====================================================
	if (handMoving && left_button) // if scene moved and left button released
	{
		// need to continue movement(animation)

		m_mousestate.mouseMoveEventHandler(event);

		int dx = 0;
		int dy = 0;
		qreal mouse_speed = 0;

		mouseSpeed_helper(mouse_speed,dx,dy,150,1900);

		if (dx!=0 || dy!=0)
			m_movement.move(-dx,-dy, scene_move_duration + mouse_speed);

	}

	if (rotating && right_button)
	{
		

		if (isWndStillExists(m_rotatingWnd))
		{

			m_rotatingWnd->drawRotationHelper(false);
			

			int dx = 0;
			int dy = 0;
			qreal mouse_speed = 0;

			mouseSpeed_helper(mouse_speed,dx,dy,150,1900);

			if (dx!=0 || dy!=0)
			{
				
				QPointF center_point = m_rotatingWnd->boundingRect().center(); // by default center is the center of the item

				//if (wnd->isFullScreen()) // if wnd is in full scree mode center must be changed to the cetre of the viewport
				//	center_point = item->mapFromScene(mapToScene(viewport()->rect().center()));

				QPointF old_point = m_rotatingWnd->mapFromScene(mapToScene(event->pos()));
				QPointF new_point = m_rotatingWnd->mapFromScene(mapToScene(event->pos() + QPoint(dx,dy)));

				QLineF old_line(center_point, old_point);
				QLineF new_line(center_point, new_point);


				qreal angle = new_line.angleTo(old_line);

				if (angle>180)
					angle = angle - 360;


				m_rotatingWnd->z_rotate_delta(center_point, angle, item_rotation_duration);
				
			}
			else
				m_rotatingWnd->update();
		}
		
	}

	//====================================================

	if (!handMoving && left_button) // if left button released and we did not move the scene, so may bee need to zoom on the item
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

						m_movement.move(mapToScene(event->pos()), m_selectedWnd ? item_select_duration : scene_move_duration);
					}

					m_scenezoom.zoom_default(m_selectedWnd ? item_select_duration : scene_zoom_duration);
				}

				// selection should be zerro anyway
				setZeroSelection();
			}
			

			if (wnd && wnd!=m_selectedWnd && left_button ) // item and left button
			{
				// new item selected 
				onNewItemSelected_helper(wnd);
			}

			/*
			// if we zoomed more that just FS
			if (wnd && wnd==m_selectedWnd && wnd->isFullScreen() && m_scenezoom.getZoom() > m_fullScreenZoom + 1e-6)
			{
				m_movement.setDuration(2000);
				m_scenezoom.setDuration(2000);

				m_movement.move(mapToScene(event->pos()));
				if (left_button)
					m_scenezoom.zoom_delta(0.1);

				if (right_button)
					m_scenezoom.zoom_delta(-0.1);

			}
			/**/

	}

	if (left_button)
		m_handMoving = 0;

	if (right_button)
	{
		m_rotationCounter = 0;
		m_rotatingWnd = 0;
	}
}


void GraphicsView::mouseDoubleClickEvent ( QMouseEvent * e )
{
	
	VideoWindow*item = static_cast<VideoWindow*>(itemAt(e->pos()));
	if (!item)
		return;

	if (e->button() == Qt::LeftButton)
	{
		if(item!=m_selectedWnd)
			return;

		toggleFullScreen_helper(item);
	}
	else if (e->button() == Qt::RightButton)
	{
		item->z_rotate_abs(QPointF(0,0), 0, item_hoverevent_duration);
	}

	

	m_ignore_release_event  = true;
}



void GraphicsView::keyPressEvent( QKeyEvent * e )
{

	VideoWindow* last_sel_wnd = getLastSelectedWnd();



	//===transform=========
	switch (e->key()) 
	{
		case Qt::Key_W:
			m_yRotate -= 1;
			updateTransform();
			break;
	}

	// ===========new item selection 
	if (e->nativeModifiers() && last_sel_wnd)
	{
		
		CLVideoCamera* cam = last_sel_wnd->getVideoCam();
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

	// ===========full screen
	if (last_sel_wnd)
	{
		CLVideoCamera* cam = last_sel_wnd->getVideoCam();

		switch (e->key()) 
		{
			case Qt::Key_Enter:
			case Qt::Key_Return:
				toggleFullScreen_helper(last_sel_wnd);
				break;
		}
	}



	//===movement============
	if (!e->nativeModifiers())
	{
		int step = 200;
		int dur = 3000;
		switch (e->key()) 
		{
		case Qt::Key_Left:
			m_movement.move(-step,0, dur);
			break;
		case Qt::Key_Right:
			m_movement.move(step,0, dur);
			break;
		case Qt::Key_Up:
			m_movement.move(0,-step, dur);
			break;
		case Qt::Key_Down:
			m_movement.move(0,step, dur);
			break;

		case Qt::Key_PageUp:
			m_movement.move(step,-step, dur);
			break;

		case Qt::Key_PageDown:
			m_movement.move(step,step, dur);
			break;

		case Qt::Key_Home:
			m_movement.move(-step,-step, dur);
			break;

		case Qt::Key_End:
			m_movement.move(-step,step, dur);
			break;

		}

	}

	//=================zoom in/ot 
	switch (e->key()) 
	{
	case Qt::Key_Equal: // plus
	case Qt::Key_Plus: // plus
		m_scenezoom.zoom_delta(0.05, 2000);
		break;
	case Qt::Key_Minus:
		m_scenezoom.zoom_delta(-0.05, 2000);


	}
	
	if (e->nativeModifiers())
	{
		switch (e->key()) 
		{
		case Qt::Key_End: // plus
			m_scenezoom.zoom_delta(0.05, 2000);
			break;

		case Qt::Key_Home:
			m_scenezoom.zoom_abs(-100, 2000); // absolute zoom out
			break;
		}

	}


	//QGraphicsView::keyPressEvent(e);


}

void GraphicsView::resizeEvent( QResizeEvent * event )
{
	if (m_selectedWnd && m_selectedWnd->isFullScreen())
		onItemFullScreen_helper(m_selectedWnd);


}

VideoWindow* GraphicsView::getLastSelectedWnd() 
{
	if (m_last_selectedWnd)
	{
		if (isWndStillExists(m_last_selectedWnd))
			return m_last_selectedWnd;

		m_last_selectedWnd = 0;
	}
		
	
	CLVideoCamera* cam = m_camLayout->getFirstCam();
	if (!cam)
		return 0;

	return static_cast<VideoWindow*>(cam->getVideoWindow());
	
}

//=====================================================

bool GraphicsView::isWndStillExists(const VideoWindow* wnd) const
{
	CLVideoCamera* cam = wnd->getVideoCam();
	if ( m_camLayout->getPos(cam) >=0 )// if still exists ( in layout)
		return true;
}

void GraphicsView::toggleFullScreen_helper(VideoWindow* wnd)
{
	if (!wnd->isFullScreen() || m_scenezoom.getZoom() > m_fullScreenZoom + 1e-6) // if item is not in full screen mode or if it's in FS and zoomed more
		onItemFullScreen_helper(wnd);
	else
	{
		// escape FS MODE
		setZeroSelection();
		wnd->setFullScreen(false);
		zoomDefault(item_select_duration*1.3);
	}
}


void GraphicsView::onNewItemSelected_helper(VideoWindow* new_wnd)
{
	setZeroSelection();

	m_selectedWnd = new_wnd;
	m_last_selectedWnd = new_wnd;


	QPointF point = m_selectedWnd->mapToScene(m_selectedWnd->boundingRect().center());


	m_selectedWnd->setSelected(true);

	m_movement.move(point, item_select_duration);
	m_scenezoom.zoom_abs(0.30, item_select_duration);


	

}


void GraphicsView::onItemFullScreen_helper(VideoWindow* wnd)
{
	//fitInView(wnd, Qt::KeepAspectRatio);

	//wnd->zoom_abs(selected_item_zoom, true);
	wnd->zoom_abs(1.0, 0);

	//viewport()->showFullScreen();



	QRectF item_rect = wnd->sceneBoundingRect();
	QRectF viewRect = viewport()->rect();

	QRectF sceneRect = matrix().mapRect(item_rect);


	qreal xratio = viewRect.width() / sceneRect.width();
	qreal yratio = viewRect.height() / sceneRect.height();

	qreal scl = qMin(xratio, yratio);

	
	QRectF unity = matrix().mapRect(QRectF(0, 0, 1, 1));
	scale(1 / unity.width(), 1 / unity.height());


	scl*=( unity.width());


	
	m_movement.move(item_rect.center(), item_select_duration/2 + 100);


	qreal zoom = m_scenezoom.scaleTozoom(scl);
	
	//scale(scl, scl);
	
	m_scenezoom.zoom_abs(zoom, item_select_duration/2 + 100);
	
	m_fullScreenZoom = zoom; // memorize full screen zoom

	wnd->setSelected(true,false); // do not animate
	m_selectedWnd = wnd;
	m_last_selectedWnd = wnd;

	wnd->setFullScreen(true);

}

void GraphicsView::mouseSpeed_helper(qreal& mouse_speed,  int& dx, int&dy, int min_speed, int speed_factor)
{
	dx = 0; dy = 0;

	qreal  mouse_speed_h, mouse_speed_v;

	m_mousestate.getMouseSpeed(mouse_speed, mouse_speed_h, mouse_speed_v);


	if (mouse_speed>min_speed)
	{
		bool sdx = (mouse_speed_h<0);
		bool sdy = (mouse_speed_v<0);

		dx = pow( abs(mouse_speed_h), 2.0)/speed_factor;
		dy = pow( abs(mouse_speed_v), 2.0)/speed_factor;

		if (sdx) dx =-dx;
		if (sdy) dy =-dy;
	}

}
