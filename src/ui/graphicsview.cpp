#include "graphicsview.h"
#include <QWheelEvent>
#include <QScrollBar>
#include "./base/log.h"
#include <math.h>
#include <QGraphicsitem>
#include "./video_cam_layout/videocamlayout.h"
#include "../base/rand.h"
#include "camera/camera.h"
#include "mainwnd.h"
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include "./animation/animated_bgr.h"
#include "settings.h"
#include "device_settings/dlg_factory.h"
#include "device_settings/device_settings_dlg.h"
#include "videoitem/video_wnd_item.h"
#include "video_cam_layout/layout_content.h"
#include "context_menu_helper.h"
#include <QInputDialog>
#include "video_cam_layout/layout_manager.h"
#include "view_drag_and_drop.h"
#include "layout_editor_wnd.h"
#include "videoitem/layout_item.h"
#include "videoitem/unmoved/unmoved_pixture_button.h"
#include "videoitem/search/search_filter_item.h"
#include "device/network_device.h"
#include "videoitem/web_item.h"


int doubl_clk_delay = qApp->doubleClickInterval()*0.75;
int item_select_duration = 700;
int item_hoverevent_duration = 300;
int scene_zoom_duration = 2500;
int scene_move_duration = 3000;

int item_rotation_duration = 2000;

qreal selected_item_zoom = 1.63;

extern QString button_home;
extern QString button_level_up;
extern QString button_magnifyingglass;

extern QPixmap cached(const QString &img);
extern int limit_val(int val, int min_val, int max_val, bool mirror);
//==============================================================================

GraphicsView::GraphicsView(QWidget* mainWnd):
QGraphicsView(),
m_xRotate(0), m_yRotate(0),
m_movement(this),
m_scenezoom(this),
m_handScrolling(false),
m_handMoving(0),
mSubItemMoving(false),
m_rotationCounter(0),
m_selectedWnd(0),
m_last_selectedWnd(0),
m_ignore_release_event(false),
m_ignore_conext_menu_event(false),
m_rotatingWnd(0),
mMainWnd(mainWnd),
m_movingWnd(0),
m_drawBkg(true),
m_logo(0),
m_animated_bckg(new CLBlueBackGround(50)),
mDeviceDlg(0),
mZerroDistance(true),
mViewStarted(false),
m_groupAnimation(0),
m_fps_frames(0),
m_seachItem(0),
m_min_scene_zoom(0.06)
{

	setScene(&m_scene);

	m_camLayout.setView(this);
	m_camLayout.setScene(&m_scene);



	setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers))); //Antialiasing
	//setViewport(new QGLWidget());
	setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform	| QPainter::TextAntialiasing); //Antialiasing
	
	//viewport()->setAutoFillBackground(false);



	setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
	//setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
	//setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
	//setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);


	//setCacheMode(QGraphicsView::CacheBackground);// slows down scene drawing a lot!!!


	setOptimizationFlag(QGraphicsView::DontClipPainter);
	setOptimizationFlag(QGraphicsView::DontSavePainterState);
	setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing); // Antialiasing?

	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	//setBackgroundBrush(QPixmap("./skin/logo.png"));
	//m_scene.setItemIndexMethod(QGraphicsScene::NoIndex);


	//setTransformationAnchor(QGraphicsView::NoAnchor);
	//setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	//setResizeAnchor(QGraphicsView::NoAnchor);
	//setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

	//setAlignment(Qt::AlignVCenter);


	QPalette palette;
	palette.setColor(backgroundRole(), app_bkr_color);
	setPalette(palette);



	connect(&mShow.mTimer, SIGNAL(timeout ()), this , SLOT(onShowTimer()) );
	connect(&m_scenezoom, SIGNAL(finished()), this , SLOT(onScneZoomFinished()) );

	mShow.mTimer.setInterval(1000);
	mShow.mTimer.start();

	setAcceptDrops(true);

}

GraphicsView::~GraphicsView()
{
	setZeroSelection(); 
	stop();
	delete m_animated_bckg;
}

void GraphicsView::setViewMode(ViewMode mode)
{
	m_viewMode = mode;
	m_camLayout.setEditable(mode == NormalView || mode == ItemsAcceptor);
}

GraphicsView::ViewMode GraphicsView::getViewMode() const
{
	return m_viewMode;
}

void GraphicsView::start()
{
	m_ignore_release_event = false;
	mViewStarted = true;

	m_camLayout.updateSceneRect();
	centerOn(getRealSceneRect().center());
	if (m_camLayout.getItemList().count())
	{
		zoomMin(0);
		fitInView(3000, 0);
	}

	enableMultipleSelection(false, true);

}

void GraphicsView::stop()
{
	mViewStarted = false;
	stopAnimation(); // stops animation 
	setZeroSelection(); 
	closeAllDlg();
}


void GraphicsView::closeAllDlg()
{
	if (mDeviceDlg)
	{
		mDeviceDlg->close();
		delete mDeviceDlg;
		mDeviceDlg = 0;
	}
}

SceneLayout& GraphicsView::getCamLayOut()
{
	return m_camLayout;
}

void GraphicsView::setRealSceneRect(QRect rect)
{
	m_realSceneRect = rect;
	m_min_scene_zoom = zoomForFullScreen_helper(rect);
	m_min_scene_zoom-=m_min_scene_zoom/10;
}

QRect GraphicsView::getRealSceneRect() const
{
	return m_realSceneRect;
}

qreal GraphicsView::getMinSceneZoom() const
{
	return m_min_scene_zoom;
}

void GraphicsView::setMinSceneZoom(qreal z) 
{
	m_min_scene_zoom = z;
}


CLAbstractSceneItem* GraphicsView::getSelectedItem() const
{
	return m_selectedWnd;
}

qreal GraphicsView::getZoom() const
{
	return m_scenezoom.getZoom();
}

void GraphicsView::setZeroSelection()
{
	if (m_selectedWnd && m_camLayout.hasSuchItem(m_selectedWnd))
	{
		m_selectedWnd->setItemSelected(false);

		CLVideoWindowItem* videoItem = 0;
		if (videoItem = m_selectedWnd->toVideoItem())
		{
			videoItem->showFPS(false);
			videoItem->setShowImagesize(false);
			videoItem->setShowInfoText(false);
		}
	
		
	}

	m_last_selectedWnd = m_selectedWnd;
	m_selectedWnd  = 0;
}

void GraphicsView::setAllItemsQuality(CLStreamreader::StreamQuality q, bool increase)
{
	cl_log.log("new quality", q, cl_logDEBUG1);
	
	QList<CLAbstractSceneItem*> wndlst = m_camLayout.getItemList();

	
	foreach (CLAbstractSceneItem* item, wndlst)
	{
		CLVideoWindowItem* videoItem = 0;
		if (!(videoItem=item->toVideoItem()))
			continue;

		CLVideoCamera* cam = videoItem->getVideoCam();

		if (increase || m_selectedWnd!=item) // can not decrease quality on selected wnd
			cam->setQuality(q, increase);
	}
	
}

//================================================================

void GraphicsView::wheelEvent ( QWheelEvent * e )
{
	if (!mViewStarted)
		return;

	showStop_helper();
	int numDegrees = e->delta() ;
	m_scenezoom.zoom_delta(numDegrees/3000.0, scene_zoom_duration, 0);
}

void GraphicsView::zoomMin(int duration)
{
	m_scenezoom.zoom_minimum(duration, 0);
}


void GraphicsView::updateTransform(qreal angle)
{
	/*
	QTransform tr;
	QPoint center = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());

	tr.translate(center.x(), center.y());
	tr.rotate(m_yRotate/10.0, Qt::YAxis);
	tr.rotate(m_xRotate/10.0, Qt::XAxis);
	tr.scale(0.05, 0.05);
	tr.translate(-center.x(), -center.y());
	setTransform(tr);
	/**/

	QTransform tr = transform();
	tr.rotate(angle/10.0, Qt::YAxis);
	//tr.rotate(angle/10.0, Qt::XAxis);
	setTransform(tr);
	addjustAllStaticItems();
	

}

void GraphicsView::onShowTimer()
{
	if (!mViewStarted) // we are about to stop this view
		return;

	if (!mShow.showTime)
	{
		mShow.mTimer.setInterval(1000);
		mShow.counrer++;
		
		if (mShow.counrer>4*60)
		{
			QList<CLAbstractSceneItem*> wndlst = m_camLayout.getItemList();
			qreal total = wndlst.size();
			if (total<2)
			{
				mShow.counrer = 0;
				return;
			}

			mShow.mTimer.stop();
			onCircle_helper(true);
			mShow.showTime = true;
			
		}
			
	}
	else // show m_fps_time
	{
		mShow.value++;
		QList<CLAbstractSceneItem*> wndlst = m_camLayout.getItemList();
		qreal total = wndlst.size();

		int i = 0;

		foreach (CLAbstractSceneItem* item, wndlst)
		{
			QPointF pos ( mShow.center.x() + cos((i / total) * 6.28 + mShow.value/100.0) * mShow.radius , mShow.center.y() + sin((i / total) * 6.28 + mShow.value/100.0) * mShow.radius ) ;

			item->setPos(pos);

			item->setRotation(item->getRotation() - 4.0/cl_get_random_val(7,10));


			++i;
		}

		// scene rotation===========
		{
			const int max_val = 800;

			if (mShow.value<max_val/2)
				updateTransform(-0.001);
			else
			{
				int mod = (mShow.value - max_val/2)%max_val;
				if (mod==0)	mShow.positive_dir = !mShow.positive_dir;

			
				if (mShow.positive_dir)
					updateTransform(0.001);
				else
					updateTransform(-0.001);
			}
		}
		// scene rotation===========


	}

}

void GraphicsView::onShowStart()
{
	//stopGroupAnimation();

	if (mShow.showTime)
	{
		mShow.mTimer.setInterval(1000/60);
		mShow.value = 0;
	}
	mShow.mTimer.start();

}

void GraphicsView::showStop_helper()
{
	mShow.counrer = 0;
	if (mShow.showTime)
	{
		mShow.showTime = 0;
		mShow.mTimer.setInterval(1000);
		if (mViewStarted) // if we are not about to stop this view
			onArrange_helper();
		mShow.mTimer.start();
	}

}

void GraphicsView::centerOn(const QPointF &pos)
{
	QGraphicsView::centerOn(pos);	
	addjustAllStaticItems();
	//viewport()->update();
}

void GraphicsView::initDecoration()
{

	//http://lisenok-kate.livejournal.com/7740.html

	bool level_up = false;


	if (m_camLayout.getContent()->getParent()) // if layout has parent add LevelUp decoration
		level_up = true;
		

	// if this is ItemsAcceptor( the layout we are editing now ) and parent is root we should not go to the root
	if (getViewMode()==GraphicsView::ItemsAcceptor && m_camLayout.getContent()->getParent() == CLSceneLayoutManager::instance().getAllLayoutsContent())
		level_up = false;

	bool home = m_camLayout.getContent()->checkDecorationFlag(LayoutContent::HomeButton);
	if (m_viewMode!=NormalView)
		home = false;


	bool magnifyingGlass = m_camLayout.getContent()->checkDecorationFlag(LayoutContent::MagnifyingGlass);

	bool serach = m_camLayout.getContent()->checkDecorationFlag(LayoutContent::SearchEdit);

	removeAllStaticItems();

	LayoutContent* cont = m_camLayout.getContent();
	CLAbstractUnmovedItem* item;

	if (home)
	{
		item = new CLUnMovedPixtureButton(button_home, 0,  global_decoration_opacity, 1.0, "./skin/home.png", 100, 100, 255);
		item->setStaticPos(QPoint(1,1));
		addStaticItem(item);
	}

	if (level_up)
	{
		item = new CLUnMovedPixtureButton(button_level_up, 0,  global_decoration_opacity, 1.0, "./skin/up.png", 100, 100, 255);
		item->setStaticPos(QPoint((100+10)*home+1,1));
		addStaticItem(item);
	}


	if (cont->checkDecorationFlag(LayoutContent::BackGroundLogo))
	{
		item = new CLUnMovedPixture("background", 0, 0.03, 0.03, "./skin/logo.png", viewport()->width(), viewport()->height(), -1);
		item->setStaticPos(QPoint(1,1));
		addStaticItem(item);
	}

	
	if (magnifyingGlass)
	{
		item = new CLUnMovedPixtureButton(button_magnifyingglass, 0,  0.4, 1.0, "./skin/try/Search.png", 100, 100, 255);
		item->setStaticPos(QPoint(viewport()->width() - 105,0));
		addStaticItem(item);
	}

	if (serach)
	{
		m_seachItem = new CLSerachEditItem(this, m_camLayout.getContent());
	}
	else
	{
		delete m_seachItem;
		m_seachItem = 0;
	}

	updateDecorations();
}

void GraphicsView::addjustAllStaticItems()
{
	foreach(CLAbstractUnmovedItem* item, m_staticItems)
	{
		item->adjust();
	}
}

void GraphicsView::addStaticItem(CLAbstractUnmovedItem* item, bool conn)
{
	m_staticItems.push_back(item);
	
	scene()->addItem(item);
	item->adjust();

	if (conn)
		connect(item, SIGNAL(onPressed(QString)), this, SLOT(onDecorationItemPressed(QString)));
}

void GraphicsView::removeStaticItem(CLAbstractUnmovedItem* item)
{
	m_staticItems.removeOne(item);
	item->adjust();
	scene()->removeItem(item);
	disconnect(item, SIGNAL(onPressed(QString)), this, SLOT(onDecorationItemPressed(QString)));

}

void GraphicsView::removeAllStaticItems()
{
	foreach(CLAbstractUnmovedItem* item, m_staticItems)
	{
		scene()->removeItem(item);
		disconnect(item, SIGNAL(onPressed(QString)), this, SLOT(onDecorationItemPressed(QString)));
		delete item;
	}

	m_staticItems.clear();
}

void GraphicsView::stopAnimation()
{
	foreach(CLAbstractSceneItem* itm, m_camLayout.getItemList())
	{
		itm->stop_animation();
	}

	
	m_scenezoom.stop();
	m_movement.stop();
	showStop_helper();
	stopGroupAnimation();
}

void GraphicsView::mousePressEvent ( QMouseEvent * event)
{
	if (!mViewStarted)
		return;

	m_yRotate = 0;

	m_scenezoom.stop();
	m_movement.stop();
	
	QGraphicsItem *item = itemAt(event->pos());
	CLAbstractSceneItem* aitem = navigationItem(item);
	
	if (item && item->parentItem() && !aitem) // item has non navigational parent 
	{
		QGraphicsView::mousePressEvent(event);
		return;
	}


	mSubItemMoving = false;

	if (aitem)
		aitem->stop_animation();

	if (aitem && event->button() == Qt::LeftButton && mDeviceDlg && mDeviceDlg->isVisible())
	{
		if (aitem->toVideoItem())
			show_device_settings_helper(aitem->getComplicatedItem()->getDevice());
	}

	if (isCTRLPressed(event))
	{
		// key might be pressed on diff view, so we do not get keypressevent here 
		enableMultipleSelection(true);
	}

	if (!isCTRLPressed(event))
	{
		// key might be pressed on diff view, so we do not get keypressevent here 
		enableMultipleSelection(false, (aitem && !aitem->isSelected()) );
	}


	if (!aitem) // click on void
	{
		if (!isCTRLPressed(event))
			enableMultipleSelection(false);
	}

	if (event->button() == Qt::MidButton)
	{
		QGraphicsView::mousePressEvent(event);
	}


	if (event->button() == Qt::LeftButton && isCTRLPressed(event)) 
	{
		// must mark window as selected( by default qt marks it om mouse release)
		if(aitem)
		{
			aitem->setSelected(true);
			m_movingWnd = 1;
		}
	}
	else if (event->button() == Qt::LeftButton && !isCTRLPressed(event)) 
	{
		//may be about to scroll the scene 
        m_handScrolling = true;
		viewport()->setCursor(Qt::ClosedHandCursor);
	}
	else if (event->button() == Qt::RightButton && !isCTRLPressed(event)) 
	{
		m_rotatingWnd = aitem;

	}

	m_mousestate.clearEvents();
	m_mousestate.mouseMoveEventHandler(event);


	QGraphicsView::mousePressEvent(event);


	showStop_helper();


}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
	if (!mViewStarted)
		return;


	QGraphicsItem *item = itemAt(event->pos());
	CLAbstractSceneItem* aitem = navigationItem(item);

	if ((item && item->parentItem() && !aitem) || mSubItemMoving) // item has non navigational parent 
	{
		QGraphicsView::mouseMoveEvent(event);
		mSubItemMoving = true;
		return;
	}


	bool left_button = event->buttons() & Qt::LeftButton;
	bool right_button = event->buttons() & Qt::RightButton;

	if (left_button && !isCTRLPressed(event)) 
	{
		//may be about to scroll the scene 
		m_handScrolling = true;
		viewport()->setCursor(Qt::ClosedHandCursor);
	}


	// scene movement 
	if (m_handScrolling && left_button) 
	{
		
		/*/

		// this code does not work well QPoint(2,2) depends on veiwport border width or so.

		QPoint delta = event->pos() - m_mousestate.getLastEventPoint();
		QPoint new_pos = viewport()->rect().center() - delta + QPoint(2,2); // rounding;
		QPointF new_scene_pos = mapToScene(new_pos);
		QRect rsr = getRealSceneRect();
		new_scene_pos.rx() = limit_val(new_scene_pos.x(), rsr.left(), rsr.right(), false);
		new_scene_pos.ry() = limit_val(new_scene_pos.y(), rsr.top(), rsr.bottom(), false);
		centerOn(new_scene_pos);
		/**/

		QPoint delta = event->pos() - m_mousestate.getLastEventPoint();
		QScrollBar *hBar = horizontalScrollBar();
		QScrollBar *vBar = verticalScrollBar();

		QPointF new_pos = mapToScene( viewport()->rect().center() - delta + QPoint(2,2) ); // rounding;
		QRect rsr = getRealSceneRect();

		if (new_pos.x() >= rsr.left() && new_pos.x() <= rsr.right())
			hBar->setValue(hBar->value() + (isRightToLeft() ? delta.x() : -delta.x()));

		if (new_pos.y() >= rsr.top() && new_pos.y() <= rsr.bottom())
			vBar->setValue(vBar->value() - delta.y());
		
		
		
		addjustAllStaticItems();

		
		
		//cl_log.log("==m_handMoving!!!=====", cl_logDEBUG1);

		++m_handMoving;
	}

	//item movement 
	if (left_button && isCTRLPressed(event) && m_scene.selectedItems().count() && m_movingWnd)
	{
		if (m_viewMode!=ItemsDonor)
		{
			m_movingWnd++;
			m_handScrolling = false; // if we pressed CTRL after already moved the scene => stop move the scene and just move items

			QPointF delta = mapToScene(event->pos()) - mapToScene(m_mousestate.getLastEventPoint());

			foreach(QGraphicsItem* itm, m_scene.selectedItems())
			{
				CLAbstractSceneItem* item = static_cast<CLAbstractSceneItem*>(itm);
				//QPointF wnd_pos = item->scenePos(); <---- this does not work coz item zoom ;case1
				QPointF wnd_pos = item->sceneBoundingRect().center();

				wnd_pos-=QPointF(item->boundingRect().width()/2, item->boundingRect().height()/2);
				item->setPos(wnd_pos+delta);
				item->setArranged(false);
			}
		}
		else
		{
			QByteArray itemData;
			QDataStream dataStream(&itemData, QIODevice::WriteOnly);
			items2DDstream(m_scene.selectedItems(), dataStream);

			QMimeData *mimeData = new QMimeData;
			mimeData->setData("hdwitness/layout-items", itemData);

			QDrag *drag = new QDrag(this);
			drag->setMimeData(mimeData);
			drag->setPixmap(cached("./skin/camera_dd_icon.png"));
			//drag->setPixmap(cached("./skin/logo.png"));

			
			drag->exec(Qt::CopyAction);
			m_scene.clearSelection();
			m_movingWnd = 0;
		}

	}


	if (m_rotatingWnd && right_button)
	{

		if (isItemStillExists(m_rotatingWnd))
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

				if (m_rotationCounter==2)
				{
					// at very beginning of the rotation
					QRectF view_scene = mapToScene(viewport()->rect()).boundingRect(); //viewport in the scene cord
					QRectF view_item = m_rotatingWnd->mapFromScene(view_scene).boundingRect(); //viewport in the item cord
					QPointF center_point_item = m_rotatingWnd->boundingRect().intersected(view_item).center(); // center of the intersection of the vewport and item

					
					m_rotatingWnd->setRotationPointCenter(center_point_item);
				}


				//if (wnd->isFullScreen()) // if wnd is in full scree mode center must be changed to the cetre of the viewport
				//	center_point = item->mapFromScene(mapToScene(viewport()->rect().center()));

				QPointF center_point = m_rotatingWnd->getRotationPointCenter();

				QPointF old_point = m_rotatingWnd->mapFromScene(mapToScene(m_mousestate.getLastEventPoint()));
				QPointF new_point = m_rotatingWnd->mapFromScene(mapToScene(event->pos()));

				QLineF old_line(center_point, old_point);
				QLineF new_line(center_point, new_point);


				m_rotatingWnd->setRotationPointHand(new_point);
				m_rotatingWnd->drawRotationHelper(true);


				qreal angle = new_line.angleTo(old_line);

				m_rotatingWnd->z_rotate_delta(center_point, angle, 0, 0);
				m_rotatingWnd->setArranged(false);
			}
			
		}
		
	}


	m_mousestate.mouseMoveEventHandler(event);
	QGraphicsView::mouseMoveEvent(event);

	showStop_helper();
}

void GraphicsView::mouseReleaseEvent ( QMouseEvent * event)
{
	if (!mViewStarted)
		return;

	if (m_ignore_release_event)
	{
		m_ignore_release_event = false;
		return;
	}

	//cl_log.log("====mouseReleaseEvent===", cl_logDEBUG1);

	QGraphicsItem *item = itemAt(event->pos());
	CLAbstractSceneItem* aitem = navigationItem(item);

	if (item && item->parentItem() && !aitem) // item has non navigational parent 
	{
		QGraphicsView::mouseReleaseEvent(event);
		return;
	}


	mSubItemMoving = false;



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

		mouseSpeed_helper(mouse_speed,dx,dy,150,5900);

		if (dx!=0 || dy!=0)
		{
			bool fullscreen = false;

			if (aitem)
			{
				// we released button on some wnd; 
				// we need to find out is it a "fullscreen mode", and if so restrict the motion
				// coz we are looking at full screen zoomed item
				// in this case we should not move to the next item



				// still we might zoomed in a lot manually and will deal with this window as with new full screen one
				QRectF wnd_rect =  aitem->sceneBoundingRect();
				QRectF viewport_rec = mapToScene(viewport()->rect()).boundingRect();

				if (wnd_rect.width() >= 1.5*viewport_rec.width() && wnd_rect.height() >= 1.5*viewport_rec.height())
				{
					// size of wnd is bigger than the size of view_port
					if (aitem != m_selectedWnd) 
						setZeroSelection();

					fullscreen = true;
					m_selectedWnd = aitem;
					m_last_selectedWnd = aitem;
				}


			}
			 
			m_movement.move(-dx,-dy, scene_move_duration + mouse_speed/1.5, fullscreen, 0);
		}

	}

	if (rotating && right_button)
	{
		
		m_ignore_conext_menu_event = true;
		

		if (isItemStillExists(m_rotatingWnd))
		{

			m_rotatingWnd->drawRotationHelper(false);
			

			int dx = 0;
			int dy = 0;
			qreal mouse_speed = 0;

			mouseSpeed_helper(mouse_speed,dx,dy,150,1900);

			if (dx!=0 || dy!=0)
			{
				
				QPointF center_point = m_rotatingWnd->getRotationPointCenter(); // by default center is the center of the item

				//if (wnd->isFullScreen()) // if wnd is in full scree mode center must be changed to the cetre of the viewport
				//	center_point = item->mapFromScene(mapToScene(viewport()->rect().center()));

				QPointF old_point = m_rotatingWnd->mapFromScene(mapToScene(event->pos()));
				QPointF new_point = m_rotatingWnd->mapFromScene(mapToScene(event->pos() + QPoint(dx,dy)));

				QLineF old_line(center_point, old_point);
				QLineF new_line(center_point, new_point);


				qreal angle = new_line.angleTo(old_line);

				if (angle>180)
					angle = angle - 360;


				m_rotatingWnd->z_rotate_delta(center_point, angle, item_rotation_duration, 0);
				
			}
			else
				m_rotatingWnd->update();
		}
		
	}

	//====================================================

	if (!handMoving && left_button && !isCTRLPressed(event) && !m_movingWnd)
	{
		// if left button released and we did not move the scene, and we did not move selected windows, so may bee need to zoom on the item

			if(!aitem) // not item and any button
			{

				// zero item selected 
				if (m_movement.isRuning() || m_scenezoom.isRuning())
				{
					// if something moves => stop moving 
					//m_movement.stop();
					//m_scenezoom.stop();
				}

				else
				{
					// move to the new point

				}

				// selection should be zerro anyway, but not if we are in fullscreen mode
				if (!m_selectedWnd || !m_selectedWnd->isFullScreen())
					setZeroSelection();
			}
			
			if (aitem && aitem!=m_selectedWnd) // item and left button
			{
				// new item selected 
				if (isItemFullScreenZoomed(aitem)) // check if wnd is manually zoomed; without double click
				{
					setZeroSelection();
					m_selectedWnd = aitem;
					m_last_selectedWnd = aitem;
					aitem->setItemSelected(true, false);
					aitem->setFullScreen(true);
				}
				else
					onNewItemSelected_helper(aitem, doubl_clk_delay);
			}
			else if (aitem && aitem==m_selectedWnd && !m_selectedWnd->isFullScreen()) // else must be here coz onNewItemSelected_helper change things 
			{
				// clicked on the same already selected item, fit in view ?
				

				if (!isItemFullScreenZoomed(aitem)) // check if wnd is manually zoomed; without double click
					fitInView(item_select_duration, doubl_clk_delay);
				else
					aitem->setFullScreen(true);

			}
			else if (aitem && aitem==m_selectedWnd && m_selectedWnd->isFullScreen() && aitem->isZoomable()) // else must be here coz onNewItemSelected_helper change things 
			{
				QPointF scene_pos = mapToScene(event->pos());
				int w = mapToScene(viewport()->rect()).boundingRect().width()/2;
				int h = mapToScene(viewport()->rect()).boundingRect().height()/2;

				// calc zoom for new rect 
				qreal zoom = zoomForFullScreen_helper(QRectF(scene_pos.x() - w/2, scene_pos.y() - h/2, w, h));

				int duration = 600;
				m_movement.move(scene_pos, duration, doubl_clk_delay);
				m_scenezoom.zoom_abs(zoom, duration, doubl_clk_delay);

			}

	}

	if (left_button && m_movingWnd>3) // if we did move selected wnds => unselect it
	{
		m_scene.clearSelection();
	}

	if (mid_button && aitem)
	{
		aitem->z_rotate_abs(aitem->getRotationPointCenter(), 0, item_hoverevent_duration, 0);
	}

	if (left_button)
	{
		m_handMoving = 0;
		if (m_movingWnd)
		{
			m_movingWnd = false;
			m_camLayout.updateSceneRect();
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
	if (!mViewStarted)
		return;

	if (m_ignore_conext_menu_event)
	{
		m_ignore_conext_menu_event = false;
		return;
	}

	CLVideoWindowItem* video_wnd = 0;
	CLVideoCamera* cam = 0;
	CLDevice* dev  = 0;


	CLAbstractSceneItem* wnd = static_cast<CLAbstractSceneItem*>(itemAt(event->pos()));
	if (!m_camLayout.hasSuchItem(wnd)) // this is decoration something
		wnd = 0;

	// distance menu 
	QMenu distance_menu;

	distance_menu.setWindowOpacity(global_menu_opacity);
	distance_menu.setTitle("Item distance");

	distance_menu.addAction(&dis_0);
	distance_menu.addAction(&dis_5);
	distance_menu.addAction(&dis_10);
	distance_menu.addAction(&dis_15);
	distance_menu.addAction(&dis_20);
	distance_menu.addAction(&dis_25);
	distance_menu.addAction(&dis_30);
	distance_menu.addAction(&dis_35);


	// layout editor
	QMenu layout_editor_menu;
	layout_editor_menu.setWindowOpacity(global_menu_opacity);
	layout_editor_menu.setTitle("Layout Editor");

	if (m_viewMode==NormalView)
		layout_editor_menu.addAction(&cm_layout_editor_editlayout);

	layout_editor_menu.addAction(&cm_layout_editor_change_t);
	layout_editor_menu.addAction(&cm_layout_editor_bgp);
	layout_editor_menu.addAction(&cm_layout_editor_bgp_sz);


	QMenu rotation_manu;
	rotation_manu.setTitle("Rotation");
	rotation_manu.addAction(&cm_rotate_0);
	rotation_manu.addAction(&cm_rotate_90);
	rotation_manu.addAction(&cm_rotate_minus90);
	rotation_manu.addAction(&cm_rotate_180);


	//===== final menu=====
	QMenu menu;
	menu.setWindowOpacity(global_menu_opacity);

	if (wnd) // video wnd
	{
		if (wnd->getType() == CLAbstractSceneItem::VIDEO)
		{
			// video item
			menu.addAction(&cm_fullscren);
			menu.addMenu(&rotation_manu);

			video_wnd = static_cast<CLVideoWindowItem*>(wnd);
			cam = video_wnd->getVideoCam();
			dev = cam->getDevice();

			if (dev->checkDeviceTypeFlag(CLDevice::NETWORK))
			{
				if (cam->isRecording())
				{
					menu.addAction(&cm_stop_recording);
				}
				else
				{
					menu.addAction(&cm_start_recording);
				}

				menu.addAction(&cm_view_recorded);
				menu.addAction(&cm_open_web_page);

			}

			
		}

		if (wnd->getType()==CLAbstractSceneItem::LAYOUT )
		{
			//layout button
			if (m_viewMode!=ItemsDonor)
				menu.addMenu(&layout_editor_menu);
		}

		if (wnd->getType()==CLAbstractSceneItem::RECORDER)
		{
			
		}

		menu.addAction(&cm_settings);
		menu.addAction(&cm_exit);
	}
	else
	{
		// on void menu...
		menu.addAction(&cm_fitinview);
		menu.addAction(&cm_arrange);

		if (m_camLayout.isEditable() && m_viewMode!=ItemsDonor)
		{
			menu.addAction(&cm_add_layout);
			//menu.addMenu(&layout_editor_menu);
		}

		menu.addMenu(&distance_menu);
		menu.addAction(&cm_togglefs);
		menu.addAction(&cm_exit);

	}


	QAction* act = menu.exec(QCursor::pos());

	//=========results===============================

	if (act==&cm_exit)
		QCoreApplication::instance()->exit();


	if (wnd==0) // on void menu
	{
		
		if (act== &cm_togglefs)
		{
			if (!mMainWnd->isFullScreen())
				mMainWnd->showFullScreen();
			else
				mMainWnd->showMaximized();
			
		}
		else if (act== &cm_fitinview)
		{
			fitInView(600, 0);
		}
		else if (act== &cm_arrange)
		{
			onArrange_helper();
		}
		else if (act== &dis_0 || act== &dis_5 || act == &dis_10 || act== &dis_15 || act== &dis_20 || act== &dis_25 || act== &dis_30 || act== &dis_35)
		{
			m_camLayout.setItemDistance(0.01 + (act->data()).toDouble() );
			onArrange_helper();
		}
		else if (act == &cm_add_layout) 
		{
			contextMenuHelper_addNewLayout();
		}

	}
	else // video item menu?
	{
		if (!m_camLayout.hasSuchItem(wnd))
			return;

		if (wnd->getType() == CLAbstractSceneItem::VIDEO)
		{
			if (act==&cm_fullscren)
				toggleFullScreen_helper(wnd);

			
			if (act==&cm_settings && dev)
				show_device_settings_helper(dev);

			if (act == &cm_start_recording && cam)
				cam->startRecording();

			if (act == &cm_stop_recording && cam)
				cam->stopRecording();

			if (act == &cm_view_recorded&& cam)
				contextMenuHelper_viewRecordedVideo(cam);

			if (act == &cm_open_web_page&& cam)
				contextMenuHelper_openInWebBroser(cam);


			if (act == &cm_rotate_90)
			{
				if (m_camLayout.hasSuchItem(wnd))
					contextMenuHelper_Rotation(wnd, 90);
			}

			if (act == &cm_rotate_minus90)
			{
				if (m_camLayout.hasSuchItem(wnd))
					contextMenuHelper_Rotation(wnd, -90);
			}

			if (act == &cm_rotate_0)
			{
				if (m_camLayout.hasSuchItem(wnd))
					contextMenuHelper_Rotation(wnd, 0);
			}

			if (act == &cm_rotate_180)
			{
				if (m_camLayout.hasSuchItem(wnd))
					contextMenuHelper_Rotation(wnd, -180);
			}

			
		}

		if (wnd->getType() == CLAbstractSceneItem::LAYOUT)
		{
			if (act == &cm_layout_editor_change_t)
				contextMenuHelper_chngeLayoutTitle(wnd);

			if (act == &cm_layout_editor_editlayout)
				contextMenuHelper_editLayout(wnd);
		}


	}

	QGraphicsView::contextMenuEvent(event);
	/**/

}

void GraphicsView::mouseDoubleClickEvent( QMouseEvent * event )
{
	if (!mViewStarted)
		return;

	QGraphicsItem *item = itemAt(event->pos());
	CLAbstractSceneItem* aitem = navigationItem(item);

	if (item && item->parentItem() && !aitem) // item has non navigational parent 
	{
		QGraphicsView::mouseDoubleClickEvent(event);
		return;
	}


	if (!aitem) // clicked on void space 
	{
		/*
		if (!mMainWnd->isFullScreen())
		{
			if (!mMainWnd->isFullScreen())
				mMainWnd->showFullScreen();
			else
				mMainWnd->showMaximized();
		}


		if (mZerroDistance)
		{
			m_old_distance = m_camLayout.getItemDistance();
			m_camLayout.setItemDistance(0.01);
			mZerroDistance = false;
		}
		else
		{
			m_camLayout.setItemDistance(m_old_distance);
			mZerroDistance = true;
		}

		
		onArrange_helper();
		/**/

		fitInView(1000, 0, CLAnimationTimeLine::SLOW_START_SLOW_END);
		
		
		return;
	}

	if (event->button() == Qt::LeftButton)
	{
		if(aitem!=m_selectedWnd)
			return;

		toggleFullScreen_helper(aitem);			
	
	}
	else if (event->button() == Qt::RightButton)
	{
		//item->z_rotate_abs(QPointF(0,0), 0, item_hoverevent_duration);
	}

	

	m_ignore_release_event  = true;

	QGraphicsView::mouseDoubleClickEvent(event);
}

void GraphicsView::dragEnterEvent ( QDragEnterEvent * event )
{

	if (m_viewMode!=ItemsAcceptor)	
	{
		event->ignore();
		return;
	}

	if (event->mimeData()->hasFormat("hdwitness/layout-items")) 
	{
		event->accept();
	}
}


void GraphicsView::dragMoveEvent ( QDragMoveEvent * event )
{
	if (m_viewMode!=ItemsAcceptor)	
	{
		event->ignore();
		return;
	}

	if (event->mimeData()->hasFormat("hdwitness/layout-items")) 
	{
		event->accept();
	}
}

void GraphicsView::dropEvent ( QDropEvent * event )
{
	bool empty_scene = m_camLayout.getItemList().count() == 0;

	if (m_viewMode!=ItemsAcceptor || !event->mimeData()->hasFormat("hdwitness/layout-items"))
	{
		event->ignore();
		return;
	}

	QByteArray itemData = event->mimeData()->data("hdwitness/layout-items");
	QDataStream dataStream(&itemData, QIODevice::ReadOnly);

	CLDragAndDropItems items;
	DDstream2items(dataStream, items);

	foreach(QString id, items.videodevices)
	{
		m_camLayout.getContent()->addDevice(id);
		m_camLayout.addDevice(id);
	}

	foreach(QString id, items.recorders)
	{
		LayoutContent* lc =  CLSceneLayoutManager::instance().createRecorderContent(id);
		m_camLayout.getContent()->addLayout(lc, false);
		m_camLayout.addDevice(id);
	}

	
	foreach(int lcp, items.layoutlinks)
	{
		LayoutContent* lc = reinterpret_cast<LayoutContent*>(lcp);
		LayoutContent* t = m_camLayout.getContent()->addLayout(lc, true);
		m_camLayout.addLayoutItem(lc->getName(), t, false);
	}
	/**/


	if (!items.isEmpty())
	{
		m_camLayout.updateSceneRect();

		if (empty_scene)
			centerOn(getRealSceneRect().center());

		fitInView(empty_scene ? 0 : 2000, 0);

		m_camLayout.setContentChanged(true);
	}
}


void GraphicsView::enableMultipleSelection(bool enable, bool unselect)
{
	if (enable)
	{
		setDragMode(QGraphicsView::RubberBandDrag);
		m_camLayout.makeAllItemsSelectable(true);

	}
	else
	{
		setDragMode(QGraphicsView::NoDrag); 
		
		if (unselect)
			m_camLayout.makeAllItemsSelectable(false);
	}
}



void GraphicsView::keyReleaseEvent( QKeyEvent * e )
{
	if (!mViewStarted)
		return;

	switch (e->key()) 
	{
	case Qt::Key_Control:
		//QMouseEvent fake(QEvent::None, QPoint(), Qt::NoButton, Qt::NoButton, Qt::NoModifier); // very dangerous!!!
		//QGraphicsView::mouseReleaseEvent(&fake); //some how need to update RubberBand area // very dangerous!!!

		//enableMultipleSelection(false, false);
		break;

	}

}

void GraphicsView::keyPressEvent( QKeyEvent * e )
{
	if (!mViewStarted)
		return;


	CLAbstractSceneItem* last_sel_item = getLastSelectedItem();
	


	//===transform=========
	switch (e->key()) 
	{
		case Qt::Key_S:
			global_show_item_text=!global_show_item_text;
			//removeAllStaticItems();
			break;

		case Qt::Key_Q:
			m_yRotate += 1;
			global_rotation_angel+=(0.01)/10;
			updateTransform(0.01);
			break;
		

		case Qt::Key_W:
			m_yRotate -= 1;
			global_rotation_angel+=(-0.01)/10;
			updateTransform(-0.01);
			break;

		case Qt::Key_E:
			m_yRotate -= 1;
			updateTransform(-global_rotation_angel*10);
			global_rotation_angel = 0;
			break;


		case Qt::Key_Control:
			enableMultipleSelection(true);
			break;

		case Qt::Key_X:
			mShow.counrer+=10*60;
			break;


	}

	// ===========new item selection 
	if (e->nativeModifiers() && last_sel_item)
	{
		
		CLAbstractSceneItem* next_item = 0;

		switch (e->key()) 
		{
		case Qt::Key_Left:
			next_item = m_camLayout.getNextLeftItem(last_sel_item);
			break;
		case Qt::Key_Right:
			next_item = m_camLayout.getNextRightItem(last_sel_item);
			break;
		case Qt::Key_Up:
			next_item = m_camLayout.getNextTopItem(last_sel_item);
			break;
		case Qt::Key_Down:
			next_item = m_camLayout.getNextBottomItem(last_sel_item);
			break;

		}

		if (next_item)
		{
			onNewItemSelected_helper(next_item, 0);
		}

	}

	// ===========full screen
	if (last_sel_item)
	{

		switch (e->key()) 
		{
			case Qt::Key_Enter:
			case Qt::Key_Return:
				toggleFullScreen_helper(last_sel_item);
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
			m_movement.move(-step,0, dur, false, 0);
			break;
		case Qt::Key_Right:
			m_movement.move(step,0, dur, false, 0);
			break;
		case Qt::Key_Up:
			m_movement.move(0,-step, dur, false, 0);
			break;
		case Qt::Key_Down:
			m_movement.move(0,step, dur, false, 0);
			break;

		case Qt::Key_PageUp:
			m_movement.move(step,-step, dur, false, 0);
			break;

		case Qt::Key_PageDown:
			m_movement.move(step,step, dur, false, 0);
			break;

		case Qt::Key_Home:
			m_movement.move(-step,-step, dur, false, 0);
			break;

		case Qt::Key_End:
			m_movement.move(-step,step, dur, false, 0);
			break;

		}

	}

	//=================zoom in/ot 
	switch (e->key()) 
	{
	case Qt::Key_Equal: // plus
	case Qt::Key_Plus: // plus
		m_scenezoom.zoom_delta(0.05, 2000, 0);
		break;
	case Qt::Key_Minus:
		
		m_scenezoom.zoom_delta(-0.05, 2000, 0);


	}
	
	if (e->nativeModifiers())
	{
		switch (e->key()) 
		{
		case Qt::Key_End: // plus
			m_scenezoom.zoom_delta(0.05, 2000, 0);
			break;

		case Qt::Key_Home:
			m_scenezoom.zoom_abs(-100, 2000, 0); // absolute zoom out
			break;
		}

	}

	
	QApplication::sendEvent(scene(), e);
	//QGraphicsView::keyPressEvent(e);


}

bool GraphicsView::isCTRLPressed(const QInputEvent* event) const
{
	return event->modifiers() & Qt::ControlModifier;
}

bool GraphicsView::isItemFullScreenZoomed(QGraphicsItem* item)
{
	QRectF scene_rect =  item->sceneBoundingRect();
	QRect item_view_rect = mapFromScene(scene_rect).boundingRect();

	if (item_view_rect.width() > viewport()->width()*1.01 || item_view_rect.height() > viewport()->height()*1.01)
		return true;

	
	return false;
}

CLAbstractSceneItem* GraphicsView::navigationItem(QGraphicsItem* item) const
{

	if (!item)
		return 0;

	QGraphicsItem* topParent = item;

	bool top_item = true;

	while(topParent->parentItem())
	{
		topParent = topParent->parentItem();
		top_item = false;
	}

	CLAbstractSceneItem* aitem = static_cast<CLAbstractSceneItem*>(topParent);
	if (!isItemStillExists(aitem))
		return 0;

	if (!top_item && !aitem->isInEventTransparetList(item)) // if this is not top level item and not in trans list 
		return 0;

	return aitem;
}


void GraphicsView::drawBackground ( QPainter * painter, const QRectF & rect )
{
	
	//QGraphicsView::drawBackground ( painter, rect );
	//=================
	m_fps_frames++;

	int diff = m_fps_time.msecsTo(QTime::currentTime());
	if (diff>1500)
	{
		int fps = m_fps_frames*1000/diff;
		if (m_viewMode==NormalView) 
			mMainWnd->setWindowTitle(QString::number(fps));
		m_fps_frames = 0;
		m_fps_time.restart();
	}
	
	//==================
	


	if (!m_drawBkg)
		return;
	
	m_animated_bckg->drawBackground(painter, rect);


	if (m_logo) m_logo->setPos(rect.topLeft());


}

CLAbstractUnmovedItem* GraphicsView::staticItemByName(QString name) const
{
	foreach(CLAbstractUnmovedItem* item, m_staticItems)
	{
		if(item->getName()==name)
			return item;
	}

	return 0;
}


void GraphicsView::updateDecorations()
{
	CLUnMovedPixture* pic = static_cast<CLUnMovedPixture*>(staticItemByName("background"));

	if (pic)
	{
		pic->setMaxSize(viewport()->width(), viewport()->height());
		QSize size = pic->getSize();

		pic->setStaticPos( QPoint( (viewport()->width() - size.width())/2, (viewport()->height() - size.height())/2) );
	}

	CLUnMovedPixture* mg = static_cast<CLUnMovedPixture*>(staticItemByName(button_magnifyingglass));
	if (mg)
	{
		mg->setStaticPos(QPoint(viewport()->width() - 105,0));
	}

	
	if (m_seachItem)
	{
		m_seachItem->resize();
	}

}

void GraphicsView::recalcSomeParams()
{

}


void GraphicsView::resizeEvent( QResizeEvent * event )
{
	if (!mViewStarted)
		return;

	updateDecorations();
	recalcSomeParams();


	if (m_selectedWnd && m_selectedWnd->isFullScreen())
		onItemFullScreen_helper(m_selectedWnd);
	else
		//fitInView(getRealSceneRect(),Qt::KeepAspectRatio);
	{

		centerOn(getRealSceneRect().center());
		fitInView(1000, 0);
	}

	foreach(CLAbstractSceneItem* item, m_camLayout.getItemList())
	{
		item->onResize();
	}

}

CLAbstractSceneItem* GraphicsView::getLastSelectedItem() 
{
	if (m_last_selectedWnd)
	{
		if (isItemStillExists(m_last_selectedWnd))
			return m_last_selectedWnd;

		m_last_selectedWnd = 0;
	}
		
	
	return m_camLayout.getCenterWnd();
	
}


void GraphicsView::onDecorationItemPressed(QString name)
{
	emit onDecorationPressed(m_camLayout.getContent(), name);
}

void GraphicsView::onScneZoomFinished()
{
	emit scneZoomFinished();
}


//=====================================================
bool GraphicsView::isItemStillExists(const CLAbstractSceneItem* wnd) const
{
	if ( m_camLayout.hasSuchItem(wnd) )// if still exists ( in layout)
		return true;

	return false;
}


void GraphicsView::toggleFullScreen_helper(CLAbstractSceneItem* wnd)
{
	if (!wnd->isFullScreen() || isItemFullScreenZoomed(wnd) ) // if item is not in full screen mode or if it's in FS and zoomed more
		onItemFullScreen_helper(wnd);
	else
	{
		// escape FS MODE
		setZeroSelection();
		wnd->setFullScreen(false);
		fitInView(1000, 0, CLAnimationTimeLine::SLOW_START_SLOW_END);
		
	}
}


void GraphicsView::onNewItemSelected_helper(CLAbstractSceneItem* new_wnd, int delay)
{

	setZeroSelection();

	m_selectedWnd = new_wnd;
	m_last_selectedWnd = new_wnd;


	QPointF point = m_selectedWnd->mapToScene(m_selectedWnd->boundingRect().center());

	m_selectedWnd->setItemSelected(true, true, delay);

	
	if (m_selectedWnd->toVideoItem())
	{
		
		CLDevice* dev = m_selectedWnd->getComplicatedItem()->getDevice();

		if (!dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT))
		{
			m_selectedWnd->toVideoItem()->setShowImagesize(true);
		}

		if (!dev->checkDeviceTypeFlag(CLDevice::ARCHIVE) && !dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT))
		{
			m_selectedWnd->toVideoItem()->showFPS(true);
			m_selectedWnd->toVideoItem()->setShowInfoText(true);
		}

	}


	m_movement.move(point, item_select_duration, delay, CLAnimationTimeLine::SLOW_START_SLOW_END);

	//===================	
	// width 1900 => zoom 0.278 => scale 0.07728
	int width = viewport()->width();
	qreal scale = 0.07728*width/1900.0;
	qreal zoom = m_scenezoom.scaleTozoom(scale) ;


	m_scenezoom.zoom_abs(zoom, item_select_duration, delay, CLAnimationTimeLine::SLOW_START_SLOW_END);


	

}

void GraphicsView::stopGroupAnimation()
{
	if (m_groupAnimation)
	{
		m_groupAnimation->stop();
		delete m_groupAnimation;
		m_groupAnimation = 0;
	}

}

void GraphicsView::onCircle_helper(bool show)
{
	if (!mViewStarted)
		return;


	stopGroupAnimation();

	QList<CLAbstractSceneItem*> wndlst = m_camLayout.getItemList();
	if (wndlst.empty())
		return;

	
	m_groupAnimation = new QParallelAnimationGroup;


	qreal total = wndlst.size();
	if (total<2) 
		return;

	
	m_camLayout.updateSceneRect();
	QRectF item_rect = m_camLayout.getSmallLayoutRect();
	mShow.center = item_rect.center();

	mShow.radius = max(item_rect.width(), item_rect.height())/8;
	

	int i = 0;

	foreach (CLAbstractSceneItem* item, wndlst)
	{
		QPropertyAnimation *anim = new QPropertyAnimation(item, "rotation");

		anim->setStartValue(item->getRotation());
		anim->setEndValue(cl_get_random_val(0, 30));

		anim->setDuration(1500 + cl_get_random_val(0, 300));

		anim->setEasingCurve(QEasingCurve::InOutBack);

		m_groupAnimation->addAnimation(anim);
	
		//=========================

		QPropertyAnimation *anim2 = new QPropertyAnimation(item, "pos");

		anim2->setStartValue(item->pos());

		anim2->setEndValue( QPointF ( mShow.center.x() + cos((i / total) * 6.28) * mShow.radius , mShow.center.y() + sin((i / total) * 6.28) * mShow.radius ) );


		anim2->setDuration(1500 + cl_get_random_val(0, 300));
		anim2->setEasingCurve(QEasingCurve::InOutBack);

		m_groupAnimation->addAnimation(anim2);

		item->setArranged(false);

		++i;
	}

	

	if (show)
		connect(m_groupAnimation, SIGNAL(finished ()), this, SLOT(onShowStart()));

	connect(m_groupAnimation, SIGNAL(finished ()), this, SLOT(fitInView()));


	m_groupAnimation->start();

}

void GraphicsView::onArrange_helper()
{
	if (!mViewStarted)
		return;

	stopGroupAnimation();

	QList<CLAbstractSceneItem*> wndlst = m_camLayout.getItemList();
	if (wndlst.empty())
		return;

	m_groupAnimation = new QParallelAnimationGroup;
	


	foreach (CLAbstractSceneItem* item, wndlst)
	{
		item->stop_animation();
		
		QPropertyAnimation *anim1 = new QPropertyAnimation(item, "rotation");

		anim1->setStartValue(item->getRotation());
		anim1->setEndValue(0);

		anim1->setDuration(1000 + cl_get_random_val(0, 300));
		anim1->setEasingCurve(QEasingCurve::InOutBack);

		m_groupAnimation->addAnimation(anim1);

		//=============

	}
	/**/

	QList<CLIdealWndPos> newPosLst = m_camLayout.calcArrangedPos();
	for (int i = 0; i < newPosLst.count();++i)
	{
		CLAbstractSceneItem* item = static_cast<CLAbstractSceneItem*>(newPosLst.at(i).item);

		QPropertyAnimation *anim2 = new QPropertyAnimation(item, "pos");
	
		anim2->setStartValue(item->pos());

		anim2->setEndValue(newPosLst.at(i).pos);

		anim2->setDuration(1000 + cl_get_random_val(0, 300));
		anim2->setEasingCurve(QEasingCurve::InOutBack);

		m_groupAnimation->addAnimation(anim2);

		item->setArranged(true);
	}


	connect(m_groupAnimation, SIGNAL(finished ()), this, SLOT(fitInView()));



	m_groupAnimation->start();
	
	
}

void GraphicsView::fitInView(int duration, int delay, CLAnimationTimeLine::CLAnimationCurve curve)
{
	stopGroupAnimation();

	
	if (m_selectedWnd && isItemStillExists(m_selectedWnd) && m_selectedWnd->isFullScreen())
	{
		m_selectedWnd->setFullScreen(false);
		m_selectedWnd->zoom_abs(1.0, 0, 0);
		update();
	}	/**/

	m_camLayout.updateSceneRect();
	QRectF item_rect = m_camLayout.getSmallLayoutRect();

	m_movement.move(item_rect.center(), duration, delay, curve);

	
	QRectF viewRect = viewport()->rect();

	QRectF sceneRect = matrix().mapRect(item_rect);


	qreal xratio = viewRect.width() / sceneRect.width();
	qreal yratio = viewRect.height() / sceneRect.height();

	qreal scl = qMin(xratio, yratio);


	QRectF unity = matrix().mapRect(QRectF(0, 0, 1, 1));
	//scale(1 / unity.width(), 1 / unity.height());


	scl*=( unity.width());





	qreal zoom = m_scenezoom.scaleTozoom(scl);

	//scale(scl, scl);

	
	m_scenezoom.zoom_abs(zoom, duration, delay, curve);

}

qreal GraphicsView::zoomForFullScreen_helper(QRectF rect) const
{
	QRectF viewRect = viewport()->rect();

	QRectF sceneRect = matrix().mapRect(rect);


	qreal xratio = viewRect.width() / sceneRect.width();
	qreal yratio = viewRect.height() / sceneRect.height();

	qreal scl = qMin(xratio, yratio);


	QRectF unity = matrix().mapRect(QRectF(0, 0, 1, 1));

	scl*=( unity.width());


	return m_scenezoom.scaleTozoom(scl);

}


void GraphicsView::onItemFullScreen_helper(CLAbstractSceneItem* wnd)
{

	//wnd->zoom_abs(selected_item_zoom, true);

	qreal wnd_zoom = wnd->getZoom();
	wnd->zoom_abs(1.0, 0, 0);


	wnd->setFullScreen(true); // must be called at very beginning of the function coz it will change boundingRect of the item (shadows removed)

	// inside setFullScreen signal can be send to stop the view.
	// so at this point view might be stoped already 
	if (!mViewStarted)
		return;

	QRectF item_rect = wnd->sceneBoundingRect();
	qreal zoom = zoomForFullScreen_helper(item_rect);

	int duration = 800;
	m_movement.move(item_rect.center(), duration, 0, CLAnimationTimeLine::SLOW_START_SLOW_END);


	//scale(scl, scl);

	
	m_scenezoom.zoom_abs(zoom, duration, 0, CLAnimationTimeLine::SLOW_START_SLOW_END);


	
	wnd->zoom_abs(wnd_zoom , 0, 0);
	wnd->zoom_abs(1.0 , duration, 0);


	

	wnd->setItemSelected(true,false); // do not animate
	m_selectedWnd = wnd;
	m_last_selectedWnd = wnd;

	

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

void GraphicsView::show_device_settings_helper(CLDevice* dev)
{
	bool open = false;
	QPoint p;

	if (mDeviceDlg && mDeviceDlg->getDevice()!=dev) // need to delete only if exists and not for this device
	{
		// already opened ( may be for another device )
		p = mDeviceDlg->pos();
		mDeviceDlg->hide();
		delete mDeviceDlg;
		mDeviceDlg = 0;
		open = true;
	}

	if (!mDeviceDlg)
	{
		mDeviceDlg = CLDeviceSettingsDlgFactory::instance().createDlg(dev);
		if (!mDeviceDlg)
			return ;

		if (open) // was open before
			mDeviceDlg->move(p);
		
		
	}

	mDeviceDlg->show();

	

}

void GraphicsView::contextMenuHelper_addNewLayout()
{
	// add new layout
	bool ok;
	QString name;
	while(true)
	{
		name = UIgetText(this, tr("New layout"), tr("Layout title:"), "", ok).trimmed();
		if (!ok)
			break;

		if (name.isEmpty())
		{
			UIOKMessage(this, "", "Empty tittle cannot be used.");
			continue;
		}

		name = QString("Layout:") + name;

		if (!m_camLayout.getContent()->hasSuchSublayoutName(name))
			break;

		UIOKMessage(this, "", "Such title layout already exists.");

	}

	if (ok)
	{
		LayoutContent* lc = CLSceneLayoutManager::instance().getNewEmptyLayoutContent(LayoutContent::LevelUp);
		lc->setName(name);

		if (m_viewMode==ItemsAcceptor) // if this is in layout editor
		{
			bool empty_layout = m_camLayout.getItemList().count() == 0;

			m_camLayout.getContent()->addLayout(lc, false);
			m_camLayout.addLayoutItem(name, lc);
			m_camLayout.setContentChanged(true);

			if (empty_layout)
			{
				m_camLayout.updateSceneRect();
				centerOn(getRealSceneRect().center());
				fitInView(0, 0);
			}
		}
		else if (m_viewMode==NormalView)
		{
			//=======
			CLLayoutEditorWnd* editor = new CLLayoutEditorWnd(lc);
			editor->setWindowModality(Qt::ApplicationModal);
			editor->exec();
			int result = editor->result();
			delete editor;

			if (result == QMessageBox::No)
			{
				delete lc;
			}
			else
			{
				bool empty_layout = m_camLayout.getItemList().count() == 0;

				m_camLayout.getContent()->addLayout(lc, false);
				m_camLayout.addLayoutItem(name, lc);
				m_camLayout.setContentChanged(true);

				if (empty_layout)
				{
					m_camLayout.updateSceneRect();
					centerOn(getRealSceneRect().center());
					fitInView(0, 0);
				}
			}

			//=======

		}

	}

}

void GraphicsView::contextMenuHelper_chngeLayoutTitle(CLAbstractSceneItem* wnd)
{
	if (wnd->getType()!=CLAbstractSceneItem::LAYOUT)
		return;

	CLLayoutItem* litem = static_cast<CLLayoutItem*>(wnd);
	LayoutContent* content = litem->getRefContent();

	bool ok;
	QString name;
	while(true)
	{
		name = UIgetText(this, tr("Rename current layout"), tr("Layout title:"), UIDisplayName(content->getName()), ok).trimmed();
		if (!ok)
			break;

		if (name.isEmpty())
		{
			UIOKMessage(this, "", "Empty tittle cannot be used.");
			continue;
		}

		name = QString("Layout:") + name;

		LayoutContent* parent = content->getParent();
		if (!parent)
			parent = CLSceneLayoutManager::instance().getAllLayoutsContent();

		if (!parent->hasSuchSublayoutName(name))
			break;

		UIOKMessage(this, "", "Such title layout already exists.");
	}

	if (ok)
	{
		content->setName(name);
		litem->setName(content->getName());
	}

}

void GraphicsView::contextMenuHelper_editLayout(CLAbstractSceneItem* wnd)
{
	if (wnd->getType()!=CLAbstractSceneItem::LAYOUT)
		return;

	CLLayoutItem* litem = static_cast<CLLayoutItem*>(wnd);
	LayoutContent* content = litem->getRefContent();

	LayoutContent* content_copy = LayoutContent::coppyLayout(content);


	CLLayoutEditorWnd* editor = new CLLayoutEditorWnd(content);
	editor->setWindowModality(Qt::ApplicationModal);
	editor->exec();
	int result = editor->result();
	delete editor;

	if (result == QMessageBox::No)
	{
		m_camLayout.getContent()->removeLayout(content, true);
		m_camLayout.getContent()->addLayout(content_copy, false);
		litem->setRefContent(content_copy);

	}
	else
	{
		delete content_copy;
	}
}

void GraphicsView::contextMenuHelper_viewRecordedVideo(CLVideoCamera* cam)
{
	cam->stopRecording();

	QString id = cam->getDevice()->getUniqueId();
	id = QString("./archive/") + id;

	m_camLayout.addDevice(id, true);
	fitInView(600, 100, CLAnimationTimeLine::SLOW_START_SLOW_END);
}

void GraphicsView::contextMenuHelper_openInWebBroser(CLVideoCamera* cam)
{
	CLNetworkDevice* net_device = static_cast<CLNetworkDevice*>(cam->getDevice());

	QString url;
	QTextStream(&url) << "http://" << net_device->getIP().toString();

	CLWebItem* wi = new CLWebItem(this, m_camLayout.getDefaultWndSize().width(), m_camLayout.getDefaultWndSize().height());
	m_camLayout.addItem(wi);
	wi->navigate(url);
	
	fitInView(600, 100, CLAnimationTimeLine::SLOW_START_SLOW_END);

}


void GraphicsView::contextMenuHelper_Rotation(CLAbstractSceneItem* wnd, qreal angle)
{
	QRectF view_scene = mapToScene(viewport()->rect()).boundingRect(); //viewport in the scene cord
	QRectF view_item = wnd->mapFromScene(view_scene).boundingRect(); //viewport in the item cord
	QPointF center_point_item = wnd->boundingRect().intersected(view_item).center(); // center of the intersection of the vewport and item
	wnd->setRotationPointCenter(center_point_item);

	wnd->z_rotate_abs(center_point_item, angle, 600, 0);

}