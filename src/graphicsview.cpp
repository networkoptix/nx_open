#include "graphicsview.h"
#include <QWheelEvent>
#include <QScrollBar>
#include "./base/log.h"
#include <math.h>
#include "../src/gui/graphicsview/qgraphicsitem.h"
#include "uvideo_wnd.h"
#include "videodisplay/video_cam_layout/videocamlayout.h"
#include "../base/rand.h"
#include "camera/camera.h"
#include <QSet>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include "mainwnd.h"
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include "videodisplay/animation/animated_bgr.h"


QString cm_exit("Exit");
QString cm_fitinview("Fit in View");
QString cm_circle("Circle");
QString cm_arrange("Arrange");
QString cm_options("Options...");
QString cm_togglefs("Toggle fullscreen");
QString cm_togglbkg("Toggle background");

int item_select_duration = 700;
int item_hoverevent_duration = 300;
int scene_zoom_duration = 2500;
int scene_move_duration = 3000;

int item_rotation_duration = 2000;

qreal selected_item_zoom = 1.63;

//==============================================================================

GraphicsView::GraphicsView(MainWnd* mainWnd):
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
m_ignore_conext_menu_event(false),
m_rotatingWnd(0),
m_movingWnd(0),
m_CTRL_pressed(false),
mVoidMenu(0,this,this),
mMainWnd(mainWnd),
m_drawBkg(true),
m_logo(0),
m_animated_bckg(new CLBlueBackGround(100))
{
	
}

GraphicsView::~GraphicsView()
{
	delete m_animated_bckg;
}

void GraphicsView::init()
{
	mVoidMenu.addItem(cm_fitinview);
	mVoidMenu.addItem(cm_circle);
	mVoidMenu.addItem(cm_arrange);
	mVoidMenu.addItem(cm_togglefs);
	mVoidMenu.addItem(cm_togglbkg);
	mVoidMenu.addItem(cm_options);
	mVoidMenu.addItem(cm_exit);


	/*
	m_logo = new QGraphicsPixmapItem(QPixmap("./skin/logo.jpg"));
	m_logo->setZValue(-1.0);
	m_logo->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
	scene()->addItem(m_logo);
	/**/

	
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
	
	QSet<CLVideoWindow*> wndlst = m_camLayout->getWndList();

	
	foreach (CLVideoWindow* wnd, wndlst)
	{
		CLVideoCamera* cam = wnd->getVideoCam();

		cam->setQuality(q, increase);
	}
	
}

//================================================================

void GraphicsView::wheelEvent ( QWheelEvent * e )
{
	befor_scene_zoom_chaged();
	int numDegrees = e->delta() ;
	m_scenezoom.zoom_delta(numDegrees/3000.0, scene_zoom_duration);
}

void GraphicsView::zoomDefault(int duration)
{
	befor_scene_zoom_chaged();
	m_scenezoom.zoom_default(duration);
}

void GraphicsView::updateTransform()
{
	QTransform tr;


	QPoint center = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());

	tr.translate(center.x(), center.y());
	tr.rotate(m_yRotate/10.0, Qt::YAxis);
	tr.rotate(m_xRotate/10.0, Qt::XAxis);
	tr.scale(0.05, 0.05);
	tr.translate(-center.x(), -center.y());

	setTransform(tr);

}


void GraphicsView::mousePressEvent ( QMouseEvent * event)
{
	m_yRotate = 0;

	m_scenezoom.stop();
	m_movement.stop();
	
	QGraphicsItem *item = itemAt(event->pos());

	if (mVoidMenu.hasSuchItem(item))
	{
		QGraphicsView::mousePressEvent(event);
		m_ignore_release_event = true; // menu button just pressed. so neeed to ignore release event
		return;
	}

	mVoidMenu.hide();


	VideoWindow* wnd = static_cast<VideoWindow*>(item);

	if (!isWndStillExists(wnd))
		wnd = 0;

	if (wnd)
		wnd->stop_animation();

	if (event->button() == Qt::MidButton)
	{
		QGraphicsView::mousePressEvent(event);
	}


	if (event->button() == Qt::LeftButton && m_CTRL_pressed)
	{
		m_movingWnd = wnd;
	}
	else if (event->button() == Qt::LeftButton && !m_CTRL_pressed) 
	{
		// Left-button press in scroll hand mode initiates hand scrolling.

        m_handScrolling = true;
		viewport()->setCursor(Qt::ClosedHandCursor);
	}
	else if (event->button() == Qt::RightButton && !m_CTRL_pressed) 
	{
		m_rotatingWnd = wnd;

	}

	m_mousestate.clearEvents();
	m_mousestate.mouseMoveEventHandler(event);


	QGraphicsView::mousePressEvent(event);

}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
	bool left_button = event->buttons() & Qt::LeftButton;
	bool right_button = event->buttons() & Qt::RightButton;

	// scene movement 
	if (m_handScrolling && left_button) 
	{
		QPoint delta = event->pos() - m_mousestate.getLastEventPoint();


		QScrollBar *hBar = horizontalScrollBar();
		QScrollBar *vBar = verticalScrollBar();
		hBar->setValue(hBar->value() + (isRightToLeft() ? delta.x() : -delta.x()));
		vBar->setValue(vBar->value() - delta.y());
		
		//cl_log.log("==m_handMoving!!!=====", cl_logDEBUG1);

		++m_handMoving;
	}

	//item movement 
	if (m_movingWnd && left_button)
	{
		if (isWndStillExists(m_movingWnd))
		{
			QPointF delta = mapToScene(event->pos()) - mapToScene(m_mousestate.getLastEventPoint());
			//QPointF wnd_pos = m_movingWnd->scenePos(); <---- this does not work coz item zoom ;case1

			QPointF wnd_pos = m_movingWnd->sceneBoundingRect().center();
			wnd_pos-=QPointF(m_movingWnd->width()/2, m_movingWnd->height()/2);
			m_movingWnd->setPos(wnd_pos+delta);
			m_movingWnd->setArranged(false);
		}
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
		
	}


	m_mousestate.mouseMoveEventHandler(event);
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

	QGraphicsItem *item = itemAt(event->pos());

	if (mVoidMenu.hasSuchItem(item))
	{
		QGraphicsView::mouseReleaseEvent(event);
		return;
	}

	mVoidMenu.hide();

	VideoWindow* wnd = static_cast<VideoWindow*>(item);
	if (!isWndStillExists(wnd))
		wnd = 0;


	bool left_button = event->button() == Qt::LeftButton;
	bool right_button = event->button() == Qt::RightButton;
	bool mid_button = event->button() == Qt::MidButton;
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
		{
			bool fullscreen = false;

			if (wnd)
			{
				// we released button on some wnd; 
				// we need to find out is it a "fullscreen mode", and if so restrict the motion
				// coz we are looking at full screen zoomed item
				// in this case we should not move to the next item



				// still we might zoomed in a lot manually and will deal with this window as with new full screen one
				QRectF wnd_rect =  wnd->sceneBoundingRect();
				QRectF viewport_rec = mapToScene(viewport()->rect()).boundingRect();

				if (wnd_rect.width() >= 1.5*viewport_rec.width() && wnd_rect.height() >= 1.5*viewport_rec.height())
				{
					// size of wnd is bigger than the size of view_port
					if (wnd != m_selectedWnd) 
						setZeroSelection();

					fullscreen = true;
					m_selectedWnd = wnd;
					m_last_selectedWnd = wnd;
				}



		

			}
			 
			m_movement.move(-dx,-dy, scene_move_duration + mouse_speed, fullscreen);
		}

	}

	if (rotating && right_button)
	{
		
		m_ignore_conext_menu_event = true;

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

	if (!handMoving && left_button && !m_CTRL_pressed) // if left button released and we did not move the scene, so may bee need to zoom on the item
	{

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

						//m_movement.move(mapToScene(event->pos()), m_selectedWnd ? item_select_duration : scene_move_duration);
					}
					//befor_scene_zoom_chaged();
					//m_scenezoom.zoom_default(m_selectedWnd ? item_select_duration : scene_zoom_duration);
				}

				// selection should be zerro anyway
				setZeroSelection();
			}
			
			if (wnd && wnd!=m_selectedWnd) // item and left button
			{
				// new item selected 
				onNewItemSelected_helper(wnd);
			}

			/*
			else if (wnd && wnd==m_selectedWnd) // selected item and left button
			{
				// new item selected 
				setZeroSelection();
				m_scenezoom.zoom_default(scene_zoom_duration);
			}
			/**/


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

	if (mid_button && wnd)
	{
		wnd->z_rotate_abs(QPointF(0,0), 0, item_hoverevent_duration);
	}

	if (left_button)
	{
		m_handMoving = 0;
		if (m_movingWnd)
		{
			reAdjustSceneRect();
			m_movingWnd =0;
		}
	}

	if (right_button)
	{
		m_rotationCounter = 0;
		m_rotatingWnd = 0;
	}

	QGraphicsView::mouseReleaseEvent(event);
}

void GraphicsView::contextMenuEvent ( QContextMenuEvent * event )
{
	if (m_ignore_conext_menu_event)
	{
		m_ignore_conext_menu_event = false;
		return;
	}

	mVoidMenu.hide();
	mVoidMenu.show(event->pos());

	/*
	QGraphicsPixmapItem* item = new QGraphicsPixmapItem(QPixmap("./skin/logo.jpg"));
	item->scale(0.2, 0.2);
	item->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
	item->setZValue(256*256);
	item->setPos(mapToScene(event->pos()));
	scene()->addItem(item);
	/**/

}

void GraphicsView::mouseDoubleClickEvent ( QMouseEvent * e )
{
	mVoidMenu.hide();
	VideoWindow*item = static_cast<VideoWindow*>(itemAt(e->pos()));
	if(!isWndStillExists(item))
		item = 0;

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
		//item->z_rotate_abs(QPointF(0,0), 0, item_hoverevent_duration);
	}

	

	m_ignore_release_event  = true;
}



void GraphicsView::keyReleaseEvent( QKeyEvent * e )
{
	mVoidMenu.hide();
	switch (e->key()) 
	{
	case Qt::Key_Control:
		m_CTRL_pressed = false;
		if (m_movingWnd)
		{
			reAdjustSceneRect();
			m_movingWnd =0;
		}
		break;

	}

}

void GraphicsView::OnMenuButton(QObject* owner, QString text)
{
	if (text == cm_exit)
	{
		QCoreApplication::instance()->exit();
	}
	else if (text == cm_togglefs)
	{
		mMainWnd->toggleFullScreen();
		m_scenezoom.zoom_delta(0.001,1)	;
	}
	else if (text == cm_togglbkg)
	{
		m_drawBkg= !m_drawBkg;
	}
	else if (text == cm_fitinview)
	{
		onFitInView_helper();
	}
	else if (text == cm_circle)
	{
		onCircle_helper();
	}
	else if (text == cm_arrange)
	{
		onArrange_helper();
	}

	
	
}

void GraphicsView::keyPressEvent( QKeyEvent * e )
{

	mVoidMenu.hide();
	VideoWindow* last_sel_wnd = getLastSelectedWnd();
	


	//===transform=========
	switch (e->key()) 
	{
		case Qt::Key_W:
			m_yRotate -= 1;
			updateTransform();
			break;

		case Qt::Key_Control:
			m_CTRL_pressed = true;
			break;

	}

	// ===========new item selection 
	if (e->nativeModifiers() && last_sel_wnd)
	{
		
		CLVideoWindow* next_wnd = 0;

		switch (e->key()) 
		{
		case Qt::Key_Left:
			next_wnd = m_camLayout->getNextLeftWnd(last_sel_wnd);
			break;
		case Qt::Key_Right:
			next_wnd = m_camLayout->getNextRightWnd(last_sel_wnd);
			break;
		case Qt::Key_Up:
			next_wnd = m_camLayout->getNextTopWnd(last_sel_wnd);
			break;
		case Qt::Key_Down:
			next_wnd = m_camLayout->getNextBottomWnd(last_sel_wnd);
			break;

		}

		if (next_wnd)
		{
			onNewItemSelected_helper(static_cast<VideoWindow*>(next_wnd));
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
		befor_scene_zoom_chaged();
		m_scenezoom.zoom_delta(0.05, 2000);
		break;
	case Qt::Key_Minus:
		befor_scene_zoom_chaged();
		m_scenezoom.zoom_delta(-0.05, 2000);


	}
	
	if (e->nativeModifiers())
	{
		switch (e->key()) 
		{
		case Qt::Key_End: // plus
			befor_scene_zoom_chaged();
			m_scenezoom.zoom_delta(0.05, 2000);
			break;

		case Qt::Key_Home:
			befor_scene_zoom_chaged();
			m_scenezoom.zoom_abs(-100, 2000); // absolute zoom out
			break;
		}

	}


	//QGraphicsView::keyPressEvent(e);


}

void GraphicsView::drawBackground ( QPainter * painter, const QRectF & rect )
{

	//=================
	static int frames = 0;
	frames++;
	static QTime time;

	int diff = time.msecsTo(QTime::currentTime());
	if (diff>1500)
	{
		int fps = frames*1000/diff;
		mMainWnd->setWindowTitle(QString::number(fps));
		frames = 0;
		time.restart();
	}
	
	//==================
	


	if (!m_drawBkg)
		return;
	
	m_animated_bckg->drawBackground(painter, rect);


	if (m_logo) m_logo->setPos(rect.topLeft());


}

void GraphicsView::resizeEvent( QResizeEvent * event )
{
	
	if (m_selectedWnd && m_selectedWnd->isFullScreen())
		onItemFullScreen_helper(m_selectedWnd);
	else
		//fitInView(getRealSceneRect(),Qt::KeepAspectRatio);
	{
		centerOn(getRealSceneRect().center());
		//updateSceneRect();
		invalidateScene();
	}

}

VideoWindow* GraphicsView::getLastSelectedWnd() 
{
	if (m_last_selectedWnd)
	{
		if (isWndStillExists(m_last_selectedWnd))
			return m_last_selectedWnd;

		m_last_selectedWnd = 0;
	}
		
	
	CLVideoWindow* wnd = m_camLayout->getCenterWnd();
	if (!wnd)
		return 0;

	return static_cast<VideoWindow*>(wnd);
	
}

void GraphicsView::reAdjustSceneRect()
{
	QRect r = m_camLayout->getLayoutRect();
	setSceneRect(0,0, 2*r.right(), 2*r.bottom());
	setRealSceneRect(r);
	update();
}

//=====================================================
bool GraphicsView::isWndStillExists(const VideoWindow* wnd) const
{
	if ( m_camLayout->hasSuchWnd(wnd) )// if still exists ( in layout)
		return true;

	return false;
}

void GraphicsView::befor_scene_zoom_chaged()
{
	mVoidMenu.hide();
}



void GraphicsView::toggleFullScreen_helper(VideoWindow* wnd)
{
	if (!wnd->isFullScreen() || m_scenezoom.getZoom() > m_fullScreenZoom + 1e-8) // if item is not in full screen mode or if it's in FS and zoomed more
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
	befor_scene_zoom_chaged();
	m_scenezoom.zoom_abs(0.278, item_select_duration);


	

}

void GraphicsView::onCircle_helper()
{
	QSet<CLVideoWindow*> wndlst = m_camLayout->getWndList();
	if (wndlst.empty())
		return;

	QParallelAnimationGroup *group = new QParallelAnimationGroup;


	qreal total = wndlst.size();
	if (total<2) 
		return;

	
	reAdjustSceneRect();
	QRectF item_rect = m_camLayout->getSmallLayoutRect();

	int radius = min(item_rect.width(), item_rect.height())/2;
	QPointF center = item_rect.center();

	int i = 0;

	foreach (CLVideoWindow* wnd, wndlst)
	{
		VideoWindow* item = static_cast<VideoWindow*>(wnd);

		QPropertyAnimation *anim = new QPropertyAnimation(item, "rotation");

		anim->setStartValue(item->getRotation());
		anim->setEndValue(cl_get_random_val(0, 30));

		anim->setDuration(1500 + cl_get_random_val(0, 300));

		anim->setEasingCurve(QEasingCurve::InOutBack);

		group->addAnimation(anim);
	
		//=========================

		QPropertyAnimation *anim2 = new QPropertyAnimation(item, "pos");

		anim2->setStartValue(item->pos());

		anim2->setEndValue( QPointF ( center.x() + cos((i / total) * 6.28) * radius , center.y() + sin((i / total) * 6.28) * radius) );


		anim2->setDuration(1500 + cl_get_random_val(0, 300));
		anim2->setEasingCurve(QEasingCurve::InOutBack);

		group->addAnimation(anim2);

		item->setArranged(false);

		++i;
	}

	connect(group, SIGNAL(finished ()), this, SLOT(onFitInView_helper()));

	group->start(QAbstractAnimation::DeleteWhenStopped);

}

void GraphicsView::onArrange_helper()
{

	QSet<CLVideoWindow*> wndlst = m_camLayout->getWndList();
	if (wndlst.empty())
		return;

	QParallelAnimationGroup *group = new QParallelAnimationGroup;
	


	foreach (CLVideoWindow* wnd, wndlst)
	{
		VideoWindow* item = static_cast<VideoWindow*>(wnd);
		
		QPropertyAnimation *anim1 = new QPropertyAnimation(item, "rotation");

		anim1->setStartValue(item->getRotation());
		anim1->setEndValue(0);

		anim1->setDuration(1000 + cl_get_random_val(0, 300));
		anim1->setEasingCurve(QEasingCurve::InOutBack);

		group->addAnimation(anim1);

		//=============

	}
	/**/

	QList<CLIdealWndPos> newPosLst = m_camLayout->calcArrangedPos();
	for (int i = 0; i < newPosLst.count();++i)
	{
		VideoWindow* item = static_cast<VideoWindow*>(newPosLst.at(i).wnd);

		QPropertyAnimation *anim2 = new QPropertyAnimation(item, "pos");
	
		anim2->setStartValue(item->pos());

		anim2->setEndValue(newPosLst.at(i).pos);

		anim2->setDuration(1000 + cl_get_random_val(0, 300));
		anim2->setEasingCurve(QEasingCurve::InOutBack);

		group->addAnimation(anim2);

		item->setArranged(true);
	}

	//connect(group, SIGNAL(finished ()), this, SLOT(reAdjustSceneRect()));
	connect(group, SIGNAL(finished ()), this, SLOT(onFitInView_helper()));



	group->start(QAbstractAnimation::DeleteWhenStopped);
	
	
}

void GraphicsView::onFitInView_helper()
{
	mVoidMenu.hide();


	if (m_selectedWnd && isWndStillExists(m_selectedWnd) && m_selectedWnd->isFullScreen())
	{
		m_selectedWnd->setFullScreen(false);
		m_selectedWnd->zoom_abs(1.0, 0);
		update();
	}

	reAdjustSceneRect();
	QRectF item_rect = m_camLayout->getSmallLayoutRect();

	m_movement.move(item_rect.center(), 600);

	
	QRectF viewRect = viewport()->rect();

	QRectF sceneRect = matrix().mapRect(item_rect);


	qreal xratio = viewRect.width() / sceneRect.width();
	qreal yratio = viewRect.height() / sceneRect.height();

	qreal scl = qMin(xratio, yratio);


	QRectF unity = matrix().mapRect(QRectF(0, 0, 1, 1));
	scale(1 / unity.width(), 1 / unity.height());


	scl*=( unity.width());



	m_movement.move(item_rect.center(), 500);


	qreal zoom = m_scenezoom.scaleTozoom(scl);

	//scale(scl, scl);

	befor_scene_zoom_chaged();
	m_scenezoom.zoom_abs(zoom, 500);

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
	
	befor_scene_zoom_chaged();
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
