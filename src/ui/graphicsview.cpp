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
#include "videoitem/unmoved_pixture_button.h"
#include "video_cam_layout/layout_content.h"
#include "context_menu_helper.h"
#include <QInputDialog>
#include "video_cam_layout/layout_manager.h"
#include "view_drag_and_drop.h"



int item_select_duration = 700;
int item_hoverevent_duration = 300;
int scene_zoom_duration = 2500;
int scene_move_duration = 3000;

int item_rotation_duration = 2000;

qreal selected_item_zoom = 1.63;

extern QString button_home;
extern QString button_level_up;
extern QPixmap cached(const QString &img);
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
m_fps_frames(0)
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
	stopAnimation();
	stopGroupAnimation();
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
	centerOn(getRealSceneRect().center());
	zoomMin(0);

	
	fitInView(3000);


	enableMultipleSelection(false, true);

}

void GraphicsView::stop()
{
	if (m_viewMode == ItemsAcceptor)
	{
		LayoutContent* root = CLSceneLayoutManager::instance().getAllLayoutsContent();


		if (m_camLayout.getContent()->getParent()==0 && !root->hasSuchSublayout(m_camLayout.getContent()))
		{
			root->addLayout(m_camLayout.getContent(), false);
		}

	}

	stopAnimation(); // stops animation 
	mViewStarted = false;
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
}

QRect GraphicsView::getRealSceneRect() const
{
	return m_realSceneRect;
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
	m_scenezoom.zoom_delta(numDegrees/3000.0, scene_zoom_duration);
}

void GraphicsView::zoomMin(int duration)
{
	m_scenezoom.zoom_minimum(duration);
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
	removeAllStaticItems();

	LayoutContent* cont = m_camLayout.getContent();
	CLAbstractUnmovedItem* item;

	if (cont->checkDecorationFlag(LayoutContent::HomeButton))
	{
		item = new CLUnMovedPixtureButton(this, button_home, "./skin/home.png", 100, 100, 255, 0.2);
		item->setStaticPos(QPoint(1,1));
		addStaticItem(item);
	}

	if (cont->checkDecorationFlag(LayoutContent::LevelUp))
	{
		item = new CLUnMovedPixtureButton(this, button_level_up, "./skin/up.png", 100, 100, 255, 0.2);
		item->setStaticPos(QPoint(100+10,1));
		addStaticItem(item);
	}


	if (cont->checkDecorationFlag(LayoutContent::BackGroundLogo))
	{

		item = new CLUnMovedPixture(this, "background", "./skin/logo.png", viewport()->width(), viewport()->height(), -1, 0.03);
		item->setStaticPos(QPoint(1,1));
		addStaticItem(item);

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

void GraphicsView::addStaticItem(CLAbstractUnmovedItem* item)
{
	m_staticItems.push_back(item);
	item->adjust();
	scene()->addItem(item);
	connect(item, SIGNAL(onPressed(QString)), this, SLOT(onDecorationItemPressed(QString)));
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
	m_scenezoom.stop();
	m_movement.stop();
	stopGroupAnimation();
	showStop_helper();
}

void GraphicsView::mousePressEvent ( QMouseEvent * event)
{
	if (!mViewStarted)
		return;


	m_yRotate = 0;

	m_scenezoom.stop();
	m_movement.stop();
	
	QGraphicsItem *item = itemAt(event->pos());


	CLAbstractSceneItem* wnd = static_cast<CLAbstractSceneItem*>(item);

	if (!isItemStillExists(wnd))
		wnd = 0;

	if (wnd)
		wnd->stop_animation();

	if (wnd && event->button() == Qt::LeftButton && mDeviceDlg && mDeviceDlg->isVisible())
	{
		if (wnd->toVideoItem())
			show_device_settings_helper(wnd->toVideoItem()->getVideoCam()->getDevice());
	}

	if (isCTRLPressed(event))
	{
		// key might be pressed on diff view, so we do not get keypressevent here 
		enableMultipleSelection(true);
	}

	if (!isCTRLPressed(event))
	{
		// key might be pressed on diff view, so we do not get keypressevent here 
		enableMultipleSelection(false, (wnd && !wnd->isSelected()) );
	}


	if (!wnd) // click on void
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
		if(wnd)
		{
			wnd->setSelected(true);
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
		m_rotatingWnd = wnd;

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
		QPoint delta = event->pos() - m_mousestate.getLastEventPoint();


		QScrollBar *hBar = horizontalScrollBar();
		QScrollBar *vBar = verticalScrollBar();
		hBar->setValue(hBar->value() + (isRightToLeft() ? delta.x() : -delta.x()));
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


	CLAbstractSceneItem* wnd = static_cast<CLAbstractSceneItem*>(item);
	if (!isItemStillExists(wnd))
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

		if (isItemStillExists(m_rotatingWnd))
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

	if (!handMoving && left_button && !isCTRLPressed(event) && !m_movingWnd)
	{
		// if left button released and we did not move the scene, and we did not move selected windows, so may bee need to zoom on the item

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

				}

				// selection should be zerro anyway
				setZeroSelection();
			}
			
			if (wnd && wnd!=m_selectedWnd) // item and left button
			{
				// new item selected 
				
				onNewItemSelected_helper(wnd, qApp->doubleClickInterval()/2);
			}

	}

	if (left_button && m_movingWnd>3) // if we did move selected wnds => unselect it
	{
		m_scene.clearSelection();
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

	CLAbstractSceneItem* wnd = static_cast<CLAbstractSceneItem*>(itemAt(event->pos()));
	if (!m_camLayout.hasSuchItem(wnd)) // this is decoration somthing
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

	if (m_camLayout.isContentChanged())
	{
		layout_editor_menu.addAction(&cm_layout_editor_save);
		layout_editor_menu.addAction(&cm_layout_editor_save_as);

	}

	if (m_viewMode==ItemsAcceptor)
	{
		layout_editor_menu.addAction(&cm_layout_editor_add_l);
		layout_editor_menu.addAction(&cm_layout_editor_change_t);
	}


	layout_editor_menu.addAction(&cm_layout_editor_bgp);
	layout_editor_menu.addAction(&cm_layout_editor_bgp_sz);



	//===== final menu=====
	QMenu menu;
	menu.setWindowOpacity(global_menu_opacity);

	if (wnd) // video wnd
	{
		menu.addAction(&cm_fullscren);
		menu.addAction(&cm_settings);
		menu.addAction(&cm_exit);
	}
	else
	{
		menu.addAction(&cm_fitinview);
		menu.addAction(&cm_arrange);

		if (m_camLayout.isEditable())
			menu.addMenu(&layout_editor_menu);

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
			mMainWnd->toggleFullScreen();
			m_scenezoom.zoom_delta(0.001,1)	;
		}
		else if (act== &cm_fitinview)
		{
			fitInView();
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
		else if (act == &cm_layout_editor_add_l)
		{
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

				m_camLayout.getContent()->addLayout(lc, false);
				m_camLayout.addLayoutItem(name, lc);
				m_camLayout.setContentChanged(true);
				
			}
		}
		else if (act == &cm_layout_editor_change_t)
		{

			bool ok;
			QString name;
			while(true)
			{
					name = UIgetText(this, tr("Rename current layout"), tr("Layout title:"), UIDisplayName(m_camLayout.getContent()->getName()), ok).trimmed();
					if (!ok)
						break;

					if (name.isEmpty())
					{
						UIOKMessage(this, "", "Empty tittle cannot be used.");
						continue;
					}

					name = QString("Layout:") + name;

					LayoutContent* parent = m_camLayout.getContent()->getParent();
					if (!parent)
						parent = CLSceneLayoutManager::instance().getAllLayoutsContent();

					if (!parent->hasSuchSublayoutName(name))
						break;

					UIOKMessage(this, "", "Such title layout already exists.");
			}

			if (ok)
			{
				m_camLayout.getContent()->setName(name);
			}

		}

	}
	else // video item menu?
	{
		if (!m_camLayout.hasSuchItem(wnd))
			return;

		if (act==&cm_fullscren)
		{
			toggleFullScreen_helper(wnd);
		}
		else if (act==&cm_settings)
		{
			if (wnd->toVideoItem())
				show_device_settings_helper(wnd->toVideoItem()->getVideoCam()->getDevice());
		}
	}

	QGraphicsView::contextMenuEvent(event);
	/**/

}

void GraphicsView::mouseDoubleClickEvent ( QMouseEvent * e )
{
	if (!mViewStarted)
		return;


	CLAbstractSceneItem*item = static_cast<CLAbstractSceneItem*>(itemAt(e->pos()));
	if(!isItemStillExists(item))
		item = 0;

	if (!item) // clicked on void space 
	{
		if (!mMainWnd->isFullScreen())
		{
			mMainWnd->toggleFullScreen();
			m_scenezoom.zoom_delta(0.001,1)	;
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
		
		return;
	}

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

	QGraphicsView::mouseDoubleClickEvent (e);
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
		m_camLayout.getContent()->addLayout(lc, true);
		m_camLayout.addLayoutItem(lc->getName(), lc, false);
	}


	if (!items.isEmpty())
	{
		m_camLayout.updateSceneRect();

		if (empty_scene)
			centerOn(getRealSceneRect().center());

		fitInView(empty_scene ? 0 : 2000);
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
		QMouseEvent fake(QEvent::None, QPoint(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
		QGraphicsView::mouseReleaseEvent(&fake); //some how need to update RubberBand area

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
		enableMultipleSelection(false, false);
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
			removeAllStaticItems();
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

bool GraphicsView::isCTRLPressed(const QInputEvent* event) const
{
	return event->modifiers() & Qt::ControlModifier;
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
		invalidateScene();
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
	if (!wnd->isFullScreen() || m_scenezoom.getZoom() > m_fullScreenZoom + 1e-8) // if item is not in full screen mode or if it's in FS and zoomed more
		onItemFullScreen_helper(wnd);
	else
	{
		// escape FS MODE
		setZeroSelection();
		wnd->setFullScreen(false);
		fitInView(800);
	}
}


void GraphicsView::onNewItemSelected_helper(CLAbstractSceneItem* new_wnd, int delay)
{
	
	setZeroSelection();

	m_selectedWnd = new_wnd;
	m_last_selectedWnd = new_wnd;


	QPointF point = m_selectedWnd->mapToScene(m_selectedWnd->boundingRect().center());

	m_selectedWnd->setItemSelected(true, true, delay);

	
	if (global_show_item_text && m_selectedWnd->toVideoItem())
	{
		
		CLDevice* dev = m_selectedWnd->toVideoItem()->getVideoCam()->getDevice();

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


	m_movement.move(point, item_select_duration, delay);

	//===================	
	// width 1900 => zoom 0.278 => scale 0.07728
	int width = viewport()->width();
	qreal scale = 0.07728*width/1900.0;
	qreal zoom = m_scenezoom.scaleTozoom(scale) ;


	m_scenezoom.zoom_abs(zoom, item_select_duration, delay);


	

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

	mShow.radius = max(item_rect.width(), item_rect.height())/2.5;
	

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

void GraphicsView::fitInView(int duration )
{
	stopGroupAnimation();

	if (m_selectedWnd && isItemStillExists(m_selectedWnd) && m_selectedWnd->isFullScreen())
	{
		m_selectedWnd->setFullScreen(false);
		m_selectedWnd->zoom_abs(1.0, 0);
		update();
	}

	m_camLayout.updateSceneRect();
	QRectF item_rect = m_camLayout.getSmallLayoutRect();

	m_movement.move(item_rect.center(), duration);

	
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

	
	m_scenezoom.zoom_abs(zoom, duration);

}

void GraphicsView::onItemFullScreen_helper(CLAbstractSceneItem* wnd)
{

	//wnd->zoom_abs(selected_item_zoom, true);

	qreal wnd_zoom = wnd->getZoom();
	wnd->zoom_abs(1.0, 0);


	wnd->setFullScreen(true); // must be called at very beginning of the function coz it will change boundingRect of the item (shadows removed)

	// inside setFullScreen signal can be send to stop the view.
	// so at this point view might be stoped already 
	if (!mViewStarted)
		return;

	QRectF item_rect = wnd->sceneBoundingRect();
	QRectF viewRect = viewport()->rect();

	QRectF sceneRect = matrix().mapRect(item_rect);


	qreal xratio = viewRect.width() / sceneRect.width();
	qreal yratio = viewRect.height() / sceneRect.height();

	qreal scl = qMin(xratio, yratio);

	
	QRectF unity = matrix().mapRect(QRectF(0, 0, 1, 1));
	//scale(1 / unity.width(), 1 / unity.height());


	scl*=( unity.width());

	int duration = item_select_duration/2 + 100;
	m_movement.move(item_rect.center(), duration);


	qreal zoom = m_scenezoom.scaleTozoom(scl);
	
	//scale(scl, scl);
	
	
	m_scenezoom.zoom_abs(zoom, duration);


	
	wnd->zoom_abs(wnd_zoom , 0);
	wnd->zoom_abs(1.0 , duration);


	
	m_fullScreenZoom = zoom; // memorize full screen zoom

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