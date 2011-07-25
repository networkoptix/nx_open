#include "graphicsview.h"
#include "./base/log.h"
#include "./video_cam_layout/videocamlayout.h"
#include "../base/rand.h"
#include "camera/camera.h"
#include "mainwnd.h"
#include "./animation/animated_bgr.h"
#include "settings.h"
#include "device_settings/dlg_factory.h"
#include "device_settings/device_settings_dlg.h"
#include "videoitem/video_wnd_item.h"
#include "video_cam_layout/layout_content.h"
#include "context_menu_helper.h"
#include "video_cam_layout/layout_manager.h"
#include "view_drag_and_drop.h"
#include "layout_editor_wnd.h"
#include "preferences_wnd.h"
#include "videoitem/layout_item.h"
#include "videoitem/unmoved/unmoved_pixture_button.h"
#include "videoitem/search/search_filter_item.h"
#include "device/network_device.h"
#include "videoitem/web_item.h"
#include "animation/group_animation.h"
#include "videoitem/grid_item.h"
#include "util.h"
#include "device_plugins/archive/archive/archive_stream_reader.h"
#include "device_plugins/archive/archive/archive_device.h"
#include "videoitem/unmoved/multipage/page_selector.h"
#include "ui/animation/property_animation.h"
#include "ui/recordingsettingswidget.h"
#include "ui/device_settings/style.h"
#include "videorecordersettings.h"

#include <QtCore/QSettings>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
#include <QTimer>
#include <QPropertyAnimation>

extern int  SLOT_WIDTH;

int doubl_clk_delay = qApp->doubleClickInterval();
int item_select_duration = 800;
int item_hoverevent_duration = 300;
int scene_zoom_duration = 2500;
int scene_move_duration = 3000;
int item_rotation_duration = 2000;
qreal selected_item_zoom = 1.63;

int decoration_size = 60;

static const int MAX_AUDIO_TRACKS = 16;

extern QString button_home;
extern QString button_level_up;
extern QString button_magnifyingglass;
extern QString button_squarelayout;
extern QString button_longlayout;
extern QString button_singleLineLayout;

extern int MAX_FPS_normal;
extern int MAX_FPS_selected;

extern qreal square_ratio;
extern qreal long_ratio;

extern QPixmap cached(const QString &img);
extern int limit_val(int val, int min_val, int max_val, bool mirror);
//==============================================================================

GraphicsView::GraphicsView(QWidget* mainWnd) :
    QGraphicsView(),
    m_xRotate(0),
    m_yRotate(0),
    m_movement(this),
    m_scenezoom(this),
    m_min_scene_zoom(0.06),
    m_rotationCounter(0),
    m_handScrolling(false),
    m_handMoving(0),
    mSubItemMoving(false),
    m_selectedWnd(0),
    m_last_selectedWnd(0),
    m_rotatingWnd(0),
    m_movingWnd(0),
    m_ignore_release_event(false),
    m_ignore_conext_menu_event(false),
    mMainWnd(mainWnd),
    mDeviceDlg(0),
    m_drawBkg(true),
    m_animated_bckg(new CLBlueBackGround(50)),
    m_logo(0),
    m_show(this),
    mSteadyShow(this),
    mZerroDistance(true),
    mViewStarted(false),
    mWheelZooming(false),
    m_fps_frames(0),
    m_seachItem(0),
    m_pageSelector(0),
    m_gridItem(0),
    m_menuIsHere(false),
    m_lastPressedItem(0)
#ifdef Q_OS_WIN
    ,m_desktopEncoder(0)
#endif
{
    m_timeAfterDoubleClick.restart();

	setScene(&m_scene);

	m_camLayout.setView(this);
	m_camLayout.setScene(&m_scene);

    QGLFormat glFormat(QGL::SampleBuffers);
    glFormat.setSwapInterval(1); // vsync


	setViewport(new QGLWidget(glFormat)); //Antialiasing
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

    //I've noticed that if I add a large number of objects to a scene and then start deleting them 
    //very quickly (commanded by a keystroke from my parent widget) I can actually crash qgraphicsscene. 
    //I'm not sure what's happening, but I get the impression that while I am deleting the objects 
    //qgraphicsscene starts a redraw/update and while it's updating I delete more objects and a 
    //pointer probably goes null and it crashes.
    //Someone suggested I turn off BSP indexing (which I did), and the problem went away.
    //http://www.qtcentre.org/threads/24825-Does-qgraphicsscene-schedule-updates-in-a-separate-thread
    m_scene.setItemIndexMethod(QGraphicsScene::NoIndex);

	//setBackgroundBrush(QPixmap(":/skin/logo.png"));
	//m_scene.setItemIndexMethod(QGraphicsScene::NoIndex);

	//setTransformationAnchor(QGraphicsView::NoAnchor);
	//setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	//setResizeAnchor(QGraphicsView::NoAnchor);
	//setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

	//setAlignment(Qt::AlignVCenter);

	QPalette palette;
	palette.setColor(backgroundRole(), app_bkr_color);
	setPalette(palette);

	connect(&m_secTimer, SIGNAL(timeout ()), this , SLOT(onSecTimer()) );
	connect(&m_scenezoom, SIGNAL(finished()), this , SLOT(onScneZoomFinished()) );

	m_secTimer.setInterval(1000);
	m_secTimer.start();

	setAcceptDrops(true);

	//=======animation====
	m_animationManager.registerAnimation(&m_movement);
	m_animationManager.registerAnimation(&m_scenezoom);

	m_gridItem = new CLGridItem(this);
	m_scene.addItem(m_gridItem);
	m_gridItem->setPos(m_camLayout.getGridEngine().getSettings().left, m_camLayout.getGridEngine().getSettings().top);

	setMinimumWidth(600);

    setFrameShape(QFrame::NoFrame);

    connect(&cm_start_video_recording, SIGNAL(triggered()), SLOT(toggleRecording()));
    cm_start_video_recording.setShortcuts(QList<QKeySequence>() << tr("Ctrl+R") << Qt::Key_MediaRecord);
	cm_start_video_recording.setShortcutContext(Qt::ApplicationShortcut);
	addAction(&cm_start_video_recording);
    connect(&cm_recording_settings, SIGNAL(triggered()), SLOT(recordingSettings()));
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
	mViewStarted = true;

	m_camLayout.updateSceneRect();
	centerOn(getRealSceneRect().center());

	if (m_camLayout.getItemList().count() && m_camLayout.getContent() != CLSceneLayoutManager::instance().introScreenLayoutContent())
	{
		zoomMin(0);

        int duration = 1000;

        if (m_camLayout.getItemList().count() && m_camLayout.getContent() == CLSceneLayoutManager::instance().startScreenLayoutContent())
            duration/=3;

		fitInView(duration, 0, SLOW_START_SLOW_END);
	}
    else if (m_camLayout.getContent() == CLSceneLayoutManager::instance().introScreenLayoutContent())
    {
        // must put THE only video in full screen mode
        if (m_camLayout.getItemList().count())
        {
            CLAbstractSceneItem* item = m_camLayout.getItemList().at(0);
            onItemFullScreen_helper(item,0);
        }

        
    }

	enableMultipleSelection(false, true);
    mSteadyShow.start();

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

QnPageSelector* GraphicsView::getPageSelector() const
{
    return m_pageSelector;
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
        if ( (videoItem = m_selectedWnd->toVideoItem()) )
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

bool GraphicsView::shouldOptimizeDrawing() const
{
    return (m_scenezoom.isRuning() && !mWheelZooming) || m_show.isShowTime();
}

//================================================================
void GraphicsView::wheelEvent ( QWheelEvent * e )
{
	if (!mViewStarted)
		return;

    if (onUserInput(true, true))
        return;

	showStop_helper();

	if (!m_camLayout.getContent()->checkIntereactionFlag(LayoutContent::Zoomable))
		return;

    QPoint unmoved_point = mapToScene(e->pos()).toPoint();
    if (!getRealSceneRect().contains(unmoved_point))
    {
        unmoved_point = QPoint(0,0);
    }

    if (m_scenezoom.isRuning() && !mWheelZooming) // if zooming and not coz of wheel event 
    {
        unmoved_point = QPoint(0,0);
    }
    else
    {
        mWheelZooming = true;
    }

	int numDegrees = e->delta() ;
    m_scenezoom.zoom_delta(numDegrees/3000.0, 1500, 0, unmoved_point, OUTCUBIC);
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
    */

	QTransform tr = transform();
	tr.rotate(angle/10.0, Qt::YAxis);
	//tr.rotate(angle/10.0, Qt::XAxis);
	setTransform(tr);
	addjustAllStaticItems();

}

void GraphicsView::onSecTimer()
{
	if (!mViewStarted) // we are about to stop this view
		return;

    if (!m_show.isShowTime())
	{

        ++m_show.m_counrer;

        if (m_show.m_counrer>4*60*60) // show will start after 4 h
		{
			if (m_camLayout.getItemList().count()<2 || !m_camLayout.getContent()->checkIntereactionFlag(LayoutContent::ShowAvalable))
			{
                m_show.m_counrer = 0;
				return;
			}

            m_show.setShowTime(true);

			onCircle_helper(true);

		}

	}

}

void GraphicsView::showStop_helper()
{
    m_show.m_counrer = 0;
    if (m_show.isShowTime())
	{
        m_show.stopAnimation();
        m_show.setShowTime(false);
		if (mViewStarted) // if we are not about to stop this view
			onArrange_helper();
	}

}

void GraphicsView::viewMove(int dx, int dy)
{

    if (qAbs(dx) > 200 || (qAbs(dy) > 200))
        return;

    m_movement.stopAnimation();

    QPoint delta(dx,dy);

    QScrollBar *hBar = horizontalScrollBar();
    QScrollBar *vBar = verticalScrollBar();

    // this code does not work well QPoint(2,2) depends on veiwport border width or so.
    QPointF new_pos = mapToScene( viewport()->rect().center() - delta + QPoint(2,2) ); // rounding;
    QRect rsr = getRealSceneRect();

    if (new_pos.x() >= rsr.left() && new_pos.x() <= rsr.right())
        hBar->setValue(hBar->value() + (isRightToLeft() ? delta.x() : -delta.x()));

    if (new_pos.y() >= rsr.top() && new_pos.y() <= rsr.bottom())
        vBar->setValue(vBar->value() - delta.y());

    addjustAllStaticItems();

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

	if (m_camLayout.getContent()->getParent() || 
        m_camLayout.getContent() == CLSceneLayoutManager::instance().getSearchLayout()) // if layout has parent add LevelUp decoration
		level_up = true;

	// if this is ItemsAcceptor( the layout we are editing now ) and parent is root we should not go to the root
	if (getViewMode()==GraphicsView::ItemsAcceptor && m_camLayout.getContent()->getParent() == CLSceneLayoutManager::instance().getAllLayoutsContent())
		level_up = false;

	bool home = m_camLayout.getContent()->checkDecorationFlag(LayoutContent::HomeButton);
	if (m_viewMode!=NormalView)
		home = false;

	bool magnifyingGlass = m_camLayout.getContent()->checkDecorationFlag(LayoutContent::MagnifyingGlass);
	bool serach = m_camLayout.getContent()->checkDecorationFlag(LayoutContent::SearchEdit);
	bool square_layout = m_camLayout.getContent()->checkDecorationFlag(LayoutContent::SquareLayout);
	bool long_layout = m_camLayout.getContent()->checkDecorationFlag(LayoutContent::LongLayout);
    bool sigle_line_layout = m_camLayout.getContent()->checkDecorationFlag(LayoutContent::SingleLineLayout);
    bool multiPageSelector = m_camLayout.getContent()->checkDecorationFlag(LayoutContent::MultiPageSelection);

	removeAllStaticItems();

	LayoutContent* cont = m_camLayout.getContent();
	CLAbstractUnmovedItem* item;

	int top_left = 0;

	if (home)
	{
        item = new CLUnMovedPixtureButton(button_home, 0,  global_decoration_opacity, 1.0, ":/skin/decorations/home.png", decoration_size, decoration_size, 255);
		item->setStaticPos(QPoint(1,1));
		addStaticItem(item);
		top_left+= (decoration_size + 5);
	}

	if (level_up)
	{
		item = new CLUnMovedPixtureButton(button_level_up, 0,  global_decoration_opacity, 1.0, ":/skin/decorations/level-up.png", decoration_size, decoration_size, 255);
		item->setStaticPos(QPoint(top_left,1));
		addStaticItem(item);
		top_left+=decoration_size;
	}

	if (cont->checkDecorationFlag(LayoutContent::BackGroundLogo))
	{
        
		item = new CLUnMovedPixture("background", 0, 0.05, 0.05, ":/skin/startscreen/no_logo_bkg.png", viewport()->width(), viewport()->height(), -100);
        //item = new CLUnMovedPixture("background", 0, 0.03, 0.03, ":/skin/logo", viewport()->width(), viewport()->height(), -100);
		item->setStaticPos(QPoint(1,1));
		addStaticItem(item);
        /**/
	}

	if (magnifyingGlass)
	{
		item = new CLUnMovedPixtureButton(button_magnifyingglass, 0,  0.4, 1.0, ":/skin/decorations/search.png", decoration_size, decoration_size, 255);
		item->setStaticPos(QPoint((viewport()->width() - decoration_size)/2,0));
		addStaticItem(item);
	}

	if (square_layout)
	{
		item = new CLUnMovedPixtureButton(button_squarelayout, 0,  global_decoration_opacity, 1.0, ":/skin/decorations/square-view.png", decoration_size, decoration_size, 255);
		item->setStaticPos(QPoint(viewport()->width() - 3.3*decoration_size,1));
		addStaticItem(item);
		top_left+=(decoration_size+10);
	}

	if (long_layout)
	{
		item = new CLUnMovedPixtureButton(button_longlayout, 0,  global_decoration_opacity, 1.0, ":/skin/decorations/horizontal-view.png", decoration_size, decoration_size, 255);
		item->setStaticPos(QPoint(viewport()->width() - 2.2*decoration_size+1,1));
		addStaticItem(item);
	}

    if (sigle_line_layout)
    {
        item = new CLUnMovedPixtureButton(button_singleLineLayout, 0,  global_decoration_opacity, 1.0, ":/skin/decorations/single-line-view.png", decoration_size, decoration_size, 255);
        item->setStaticPos(QPoint(viewport()->width() - 1.1*decoration_size+1,1));
        addStaticItem(item);
    }

	/**/

	if (serach)
	{
#ifdef _WIN32
		m_seachItem = new CLSerachEditItem(this, this, m_camLayout.getContent());
#else
		// There is a problem on Mac OS X with embedding control into scene.
		// As a temporary solution we are using separate window.
		m_seachItem = new CLSerachEditItem(this, 0, m_camLayout.getContent());
#endif
	}
	else
	{
		delete m_seachItem;
		m_seachItem = 0;
	}


    if (multiPageSelector)
    {
        m_pageSelector = new QnPageSelector("page_selector", 0, 0.5, 1.0);
        addStaticItem(m_pageSelector, false);
        connect(m_pageSelector, SIGNAL(onNewPageSlected(int)), &m_camLayout, SLOT(onNewPageSelected(int)) );
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
    Q_ASSERT(!m_staticItems.contains(item));

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

    if (m_pageSelector)
        disconnect(m_pageSelector, SIGNAL(onNewPageSlected(int)), &m_camLayout, SLOT(onNewPageSelected(int)) );

	foreach(CLAbstractUnmovedItem* item, m_staticItems)
	{
		scene()->removeItem(item);
		disconnect(item, SIGNAL(onPressed(QString)), this, SLOT(onDecorationItemPressed(QString)));
		delete item;
	}

	m_staticItems.clear();
    m_pageSelector = 0;
}

void GraphicsView::stopAnimation()
{
	QList<CLAbstractSceneItem*> lst = m_camLayout.getItemList();
    
    foreach(CLAbstractSceneItem* itm, lst)
	{
		itm->stop_animation();
	}

	m_animationManager.stopAllAnimations();
	showStop_helper();
    //mSteadyShow.stopAnimation();

}

void GraphicsView::mousePressEvent ( QMouseEvent * event)
{
	if (!mViewStarted)
		return;

    if (m_timeAfterDoubleClick.elapsed() < doubl_clk_delay)
        return; // ignore some accident mouse click accidents after double click

    m_ignore_release_event = false;

	if (m_gridItem->isVisible() && !isCTRLPressed(event))
		m_gridItem->hide();

    if (onUserInput(true, true))
        return;
    

	m_yRotate = 0;

	stopAnimation();

	QGraphicsItem *item = itemAt(event->pos());
	CLAbstractSceneItem* aitem = navigationItem(item);

    m_lastPressedItem = aitem;

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
	else if (event->button() == Qt::LeftButton && !isCTRLPressed(event) && !isALTPressed(event)) 
	{
		//may be about to scroll the scene 
        m_handScrolling = true;
		viewport()->setCursor(Qt::ClosedHandCursor);
	}
	else if ((event->button() == Qt::RightButton && !isCTRLPressed(event))
			 || (event->button() == Qt::LeftButton && isALTPressed(event))) 
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

	if (m_gridItem->isVisible() && !isCTRLPressed(event))
		m_gridItem->hide();

    if (onUserInput(true, false))
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

	if (left_button && !isCTRLPressed(event) && !isALTPressed(event)) 
	{
		//may be about to scroll the scene 
		m_handScrolling = true;
		viewport()->setCursor(Qt::ClosedHandCursor);
	}

	// scene movement 
	if (m_handScrolling && left_button && 	m_camLayout.getContent()->checkIntereactionFlag(LayoutContent::SceneMovable))
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
        */

		QPoint delta = event->pos() - m_mousestate.getLastEventPoint();
        viewMove(delta.x(), delta.y());

		//cl_log.log("==m_handMoving!!!=====", cl_logDEBUG1);

		++m_handMoving;
	}

	//item movement 
	if (left_button && isCTRLPressed(event) && !isALTPressed(event) && m_scene.selectedItems().count() && m_movingWnd && m_camLayout.getContent()->checkIntereactionFlag(LayoutContent::ItemMovable))
	{
		if (m_viewMode!=ItemsDonor)
		{
			m_movingWnd++;
			m_handScrolling = false; // if we pressed CTRL after already moved the scene => stop move the scene and just move items

			QPointF delta = mapToScene(event->pos()) - mapToScene(m_mousestate.getLastEventPoint());

            QList<QGraphicsItem *> lst = m_scene.selectedItems();
			
            foreach(QGraphicsItem* itm, lst)
			{
				CLAbstractSceneItem* item = navigationItem(itm);
                if (!item)
                    continue;

				//QPointF wnd_pos = item->scenePos(); //<---- this does not work coz item zoom ;case1
				QPointF wnd_pos = item->sceneBoundingRect().center();
				wnd_pos-=QPointF(item->boundingRect().width()/2, item->boundingRect().height()/2);

				if (m_movingWnd==2) // if this is a first movement
				{
					item->setOriginallyArranged(item->isArranged());
					item->setOroginalPos(wnd_pos);
				}

				item->setPos(wnd_pos+delta);
				item->setArranged(false);

				item->setZValue(global_base_scene_z_level + 2); // moving items should be on top

				if (m_camLayout.getGridEngine().getItemToSwapWith(item) || m_camLayout.getGridEngine().canBeDropedHere(item))
					item->setCanDrop(true);
				else
					item->setCanDrop(false);

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
			drag->setPixmap(cached(":/skin/camera_dd_icon.png"));
			//drag->setPixmap(cached(":/skin/logo.png"));

			drag->exec(Qt::CopyAction);
			m_scene.clearSelection();
			m_movingWnd = 0;
		}

	}

	if (m_rotatingWnd && (right_button || (left_button && isALTPressed(event))) && m_camLayout.getContent()->checkIntereactionFlag(LayoutContent::ItemRotatable))
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

    if (m_timeAfterDoubleClick.elapsed() < doubl_clk_delay)
        return; // ignore some accident mouse click accidents after double click


    if (onUserInput(true, true))
        return;

	if (m_ignore_release_event)
	{
		m_ignore_release_event = false;
		return;
	}

	//cl_log.log("====mouseReleaseEvent===", cl_logDEBUG1);

	QGraphicsItem *item = itemAt(event->pos());
	CLAbstractSceneItem* aitem = navigationItem(item);

	if ((item && item->parentItem() && !aitem) || (aitem!=m_lastPressedItem)) // item has non navigational parent 
	{
		QGraphicsView::mouseReleaseEvent(event);
		return;
	}

	mSubItemMoving = false;

	bool left_button = event->button() == Qt::LeftButton;
//	bool right_button = event->button() == Qt::RightButton;
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

	if (rotating /*&& (right_button || (left_button && isALTPressed(event)))*/)
	{

		m_ignore_conext_menu_event = true;

		if (isItemStillExists(m_rotatingWnd))
		{

			m_rotatingWnd->drawRotationHelper(false);

			int dx = 0;
			int dy = 0;
			qreal mouse_speed = 0;

			mouseSpeed_helper(mouse_speed,dx,dy,150,5900);

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

	if (!handMoving && left_button && !isCTRLPressed(event) && !isALTPressed(event) && !m_movingWnd)
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
					fitInView(item_select_duration + 100, doubl_clk_delay, SLOW_START_SLOW_END);
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

				int duration = 800;
				m_movement.move(scene_pos, duration, doubl_clk_delay, SLOW_START_SLOW_END);
				m_scenezoom.zoom_abs(zoom, duration, doubl_clk_delay, QPoint(0,0), SLOW_START_SLOW_END);

			}

	}

	if (left_button && m_movingWnd) 
	{
		if (m_movingWnd>3)// if we did move selected wnds => unselect it
		{
			navigation_grid_items_drop_helper();
		}

		if (m_movingWnd)
		{
			m_movingWnd = 0;
			m_camLayout.updateSceneRect();
		}

	}

	if (mid_button && aitem)
	{
		aitem->z_rotate_abs(aitem->getRotationPointCenter(), 0, item_hoverevent_duration, 0);
	}

	if (left_button)
	{
		m_handMoving = 0;
	}

//	if (right_button || (left_button && isALTPressed(event)))
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

	CLAbstractSceneItem* aitem = navigationItem(itemAt(event->pos()));
	if (aitem)
		aitem->drawRotationHelper(false);

	// distance menu 
	QMenu distance_menu;

	//distance_menu.setWindowOpacity(global_menu_opacity);
	distance_menu.setTitle("Item distance");

	distance_menu.addAction(&dis_0);
	distance_menu.addAction(&dis_5);
	distance_menu.addAction(&dis_10);
	distance_menu.addAction(&dis_15);
	distance_menu.addAction(&dis_20);
	distance_menu.addAction(&dis_25);
	distance_menu.addAction(&dis_30);
	distance_menu.addAction(&dis_35);


	QMenu rotation_manu;
	rotation_manu.setTitle("Rotation");
	rotation_manu.addAction(&cm_rotate_0);
	rotation_manu.addAction(&cm_rotate_90);
	rotation_manu.addAction(&cm_rotate_minus90);
	rotation_manu.addAction(&cm_rotate_180);


    QMenu audioMenu;
    audioMenu.setTitle("Audio tracks");
    /*
    QAction cmAudio0(0);
    QAction cmAudio1(0);
    QAction cmAudio2(0);
    QAction cmAudio3(0);
    QAction* AudioTracks[12] = {&cmAudio0, &cmAudio1, &cmAudio2,  &cmAudio3,
                                &cmAudio4, &cmAudio5, &cmAudio6,  &cmAudio7,
                                &cmAudio8, &cmAudio9, &cmAudio10, &cmAudio11};
    cmAudio0.setCheckable(true);
    cmAudio1.setCheckable(true);
    cmAudio2.setCheckable(true);
    cmAudio3.setCheckable(true);
    */
    /*
    QAction* AudioTracks[MAX_AUDIO_TRACKS];
    for (int i = 0; i < MAX_AUDIO_TRACKS; ++i)
    {
        AudioTracks[i] = new QAction(0);
        AudioTracks[i]->setCheckable(true);
    }
    */

    QVector<QAction*> AudioTracks;

	//===== final menu=====
	QMenu menu;
	//menu.setWindowOpacity(global_menu_opacity);

	if (aitem && m_scene.selectedItems().count()==0) // video wnd and single selection
	{
		if (aitem->getType() == CLAbstractSceneItem::VIDEO)
		{
			// video item
			menu.addAction(&cm_fullscren);
            menu.addAction(&cm_remove_from_layout);
			menu.addMenu(&rotation_manu);

			video_wnd = static_cast<CLVideoWindowItem*>(aitem);
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

                if (contextMenuHelper_existRecordedVideo(cam))
				    menu.addAction(&cm_view_recorded);

				menu.addAction(&cm_open_web_page);
			}

            if (dev->checkDeviceTypeFlag(CLDevice::ARCHIVE))
            {
                CLAbstractArchiveReader* reader = static_cast<CLAbstractArchiveReader*>(cam->getStreamreader());
                QStringList tracks = reader->getAudioTracksInfo();
                if (tracks.size() > 1 ) // if we've got more than one channel
                {
                    

                    for (int i = 0; i  < qMin(tracks.size(), MAX_AUDIO_TRACKS); ++i )
                    {
                        AudioTracks << new QAction(0);
                        AudioTracks[i]->setCheckable(true);
                        AudioTracks[i]->setText(QString("  ") + tracks.at(i));
                        audioMenu.addAction(AudioTracks[i]);
                    }

                    unsigned int selectedAudio = qMin((unsigned int)MAX_AUDIO_TRACKS-1, reader->getCurrentAudioChannel());
                    AudioTracks[selectedAudio]->setChecked(true);


                    menu.addMenu(&audioMenu);
                }
            }

            if (dev->checkDeviceTypeFlag(CLDevice::ARCHIVE) || dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT))
            {
                menu.addAction(&cm_open_containing_folder);
            }

            


            if (dev->checkDeviceTypeFlag(CLDevice::RECORDED) && !dev->getUniqueId().contains(getRecordingDir()))
            {
                menu.addAction(&cm_save_recorded_as);
            }

            menu.addAction(&cm_settings);

		}

		if (aitem->getType()==CLAbstractSceneItem::LAYOUT )
		{
			menu.addAction(&cm_layout_editor_change_t);
            menu.addAction(&cm_remove_from_layout);
		}

		if (aitem->getType()==CLAbstractSceneItem::RECORDER)
		{

		}

		
	}
    else if (aitem && m_scene.selectedItems().count()>0)
    {
        // multiple selection
        menu.addAction(&cm_remove_from_layout);
        bool haveAtLeastOneNonrecordingCam = false;
        bool haveAtLeastOneRecordingCam = false;


        foreach(QGraphicsItem* item, m_scene.selectedItems())
        {

            CLAbstractSceneItem* nav_item = navigationItem(item);;
            if (!nav_item)
                continue;
            
            CLAbstractComplicatedItem* ca  = nav_item->getComplicatedItem();

            if (!ca)
                continue;

            if (!ca->getDevice()->checkDeviceTypeFlag(CLDevice::NETWORK))
                continue;

            if (ca->getDevice()->checkDeviceTypeFlag(CLDevice::RECORDED))
                continue;


            CLVideoCamera* cam = static_cast<CLVideoCamera*>(ca);

            if (cam->isRecording())
                haveAtLeastOneRecordingCam = true;
            else
                haveAtLeastOneNonrecordingCam = true;
        }

        if (haveAtLeastOneNonrecordingCam)
            menu.addAction(&cm_start_recording);

        if (haveAtLeastOneRecordingCam )
            menu.addAction(&cm_stop_recording);



    }
    else
    {
        // on void menu...
        if (m_camLayout.getContent() != CLSceneLayoutManager::instance().startScreenLayoutContent())
        {
            menu.addAction(&cm_start_video_recording);
            menu.addAction(&cm_recording_settings);
            menu.addSeparator();
            menu.addAction(&cm_fitinview);
            menu.addAction(&cm_arrange);

            if (m_camLayout.isEditable() && m_viewMode!=ItemsDonor)
            {
                //menu.addAction(&cm_add_layout); // major functions disabled 
                //menu.addMenu(&layout_editor_menu);
            }

            bool saved_content = false;
            LayoutContent* current =  m_camLayout.getContent();
            while(1)
            {
                current = current->getParent();
                if (current==0)
                    break;

                if (current == CLSceneLayoutManager::instance().getAllLayoutsContent())
                {
                    saved_content = true;
                    break;
                }
            }

            if (saved_content)
                menu.addAction(&cm_restore_layout);
                

            if (m_camLayout.getContent() != CLSceneLayoutManager::instance().getAllLayoutsContent())
            {
                if (m_camLayout.getContent() != CLSceneLayoutManager::instance().getSearchLayout())
                    menu.addAction(&cm_save_layout);

                menu.addAction(&cm_save_layout_as);
            }

            menu.addMenu(&distance_menu);
        }
        
        menu.addAction(&cm_togglefs);
        menu.addAction(&cm_preferences);
        menu.addSeparator();
        menu.addAction(&cm_exit);

    }

    m_menuIsHere = true;
    QAction* act = menu.exec(QCursor::pos());
    m_menuIsHere = false;

	//=========results===============================

	if (act==&cm_exit)
		QCoreApplication::instance()->exit();

	if (aitem==0) // on void menu
	{

		if (act== &cm_togglefs)
		{
			if (!mMainWnd->isFullScreen())
				mMainWnd->showFullScreen();
			else
				mMainWnd->showMaximized();
		}
        else if (act == &cm_preferences)
        {
            PreferencesWindow* preferencesDialog = new PreferencesWindow();
            preferencesDialog->exec();
            delete preferencesDialog;
        }
		else if (act== &cm_fitinview)
		{
			fitInView(700, 0, SLOW_START_SLOW_END);
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
        else if (act == &cm_restore_layout)
        {
            contextMenuHelper_restoreLayout();
        }
        else if (act == &cm_save_layout_as) 
        {
            contextMenuHelper_saveLayout(true);
        }
        else if (act == &cm_save_layout)
        {
            contextMenuHelper_saveLayout(false);
        }


	}
	else if (aitem && m_scene.selectedItems().count()==0 )// video item single selection
	{
		if (!m_camLayout.hasSuchItem(aitem))
			return;

		if (aitem->getType() == CLAbstractSceneItem::VIDEO)
		{

            if (dev->checkDeviceTypeFlag(CLDevice::ARCHIVE))
            {
                CLAbstractArchiveReader* reader = static_cast<CLAbstractArchiveReader*>(cam->getStreamreader());

                QStringList tracks = reader->getAudioTracksInfo();
                if (tracks.size() > 1 ) // if we've got more than one channel
                {
                    if (act == AudioTracks[0])
                        reader->setAudioChannel(0);

                    if (act == AudioTracks[1])
                        reader->setAudioChannel(1);

                    if (act == AudioTracks[2])
                        reader->setAudioChannel(2);

                    if (act == AudioTracks[3])
                        reader->setAudioChannel(3);
                }

            }

            if (dev->checkDeviceTypeFlag(CLDevice::ARCHIVE) || dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT))
            {
                
                if (act == &cm_open_containing_folder)
                {
                    
                    QString file = dev->getUniqueId();

#ifdef WIN32
                    file = file.replace('/', '\\');
                    QString program;
                    QTextStream(&program) << "explorer.exe /select," << file;
                    WinExec(program.toLatin1(),SW_SHOW);
#endif

                    
                }
                

            }



			if (act==&cm_fullscren)
				toggleFullScreen_helper(aitem);

			if (act==&cm_settings && dev)
				show_device_settings_helper(dev);

			if (act == &cm_start_recording && cam)
            {
                CLAbstractComplicatedItem* recorded =  m_camLayout.haveRecordedVideoPlayingFor(cam);
                if (recorded)
                {
                    QList<CLAbstractSubItemContainer*> itemlst;
                    itemlst.push_back(recorded->getSceneItem());
                    m_camLayout.removeItems(itemlst, true);
                }

				cam->startRecording();
            }

			if (act == &cm_stop_recording && cam)
				cam->stopRecording();

			if (act == &cm_view_recorded&& cam)
				contextMenuHelper_viewRecordedVideo(cam);

            if (act == &cm_save_recorded_as && cam)
                contextMenuHelper_saveRecordedAs(cam);
            

			if (act == &cm_open_web_page&& cam)
				contextMenuHelper_openInWebBroser(cam);

			if (act == &cm_rotate_90)
			{
				if (m_camLayout.hasSuchItem(aitem))
					contextMenuHelper_Rotation(aitem, 90);
			}

			if (act == &cm_rotate_minus90)  
			{
				if (m_camLayout.hasSuchItem(aitem))
					contextMenuHelper_Rotation(aitem, -90);
			}

			if (act == &cm_rotate_0)
			{
				if (m_camLayout.hasSuchItem(aitem))
					contextMenuHelper_Rotation(aitem, 0);
			}

			if (act == &cm_rotate_180)
			{
				if (m_camLayout.hasSuchItem(aitem))
					contextMenuHelper_Rotation(aitem, -180);
			}

		}

		if (aitem->getType() == CLAbstractSceneItem::LAYOUT)
		{
			if (act == &cm_layout_editor_change_t)
				contextMenuHelper_chngeLayoutTitle(aitem);

			if (act == &cm_layout_editor_editlayout)
				contextMenuHelper_editLayout(aitem);
		}

        if (act == &cm_remove_from_layout)
        {
            QList<CLAbstractSubItemContainer*> lst;
            lst.push_back(aitem);
            m_camLayout.removeItems(lst, true);
        }

	}
    else if (aitem && m_scene.selectedItems().count()>0 )// video item multiple selection
    {

        if (act == &cm_remove_from_layout)
        {
            QList<CLAbstractSubItemContainer*> lst;

            foreach(QGraphicsItem* item, m_scene.selectedItems())
            {
                CLAbstractSubItemContainer* aitemc = static_cast<CLAbstractSubItemContainer*>(item);
                lst.push_back(aitemc);
            }
            
            m_camLayout.removeItems(lst, true);
        }

        
        if (act == &cm_start_recording || act == &cm_stop_recording)
        {
            foreach(QGraphicsItem* item, m_scene.selectedItems())
            {
                CLAbstractSceneItem* nav_item = navigationItem(item);;
                if (!nav_item)
                    continue;

                CLAbstractComplicatedItem* ca  = nav_item->getComplicatedItem();

                if (!ca)
                    continue;

                if (!ca->getDevice()->checkDeviceTypeFlag(CLDevice::NETWORK))
                    continue;

                CLVideoCamera* cam = static_cast<CLVideoCamera*>(ca);

                if (act == &cm_start_recording)
                {
                    if (cam->isRecording())
                        continue;

                    CLAbstractComplicatedItem* recorded =  m_camLayout.haveRecordedVideoPlayingFor(cam);
                    if (recorded)
                    {
                        QList<CLAbstractSubItemContainer*> itemlst;
                        itemlst.push_back(recorded->getSceneItem());
                        m_camLayout.removeItems(itemlst, true);
                    }

                    cam->startRecording();

                }

                if (act == &cm_stop_recording)
                {
                    cam->stopRecording();
                }

            }

        }

    }


	QGraphicsView::contextMenuEvent(event);
	/**/
    foreach(QAction* action, AudioTracks)
        delete action;
}

void GraphicsView::mouseDoubleClickEvent( QMouseEvent * event )
{
	if (!mViewStarted)
		return;

    m_timeAfterDoubleClick.restart();

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
        */

		fitInView(1500, 0, SLOW_START_SLOW_END);

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
		m_camLayout.addDevice(id, true);
	}

	foreach(QString id, items.recorders)
	{
		LayoutContent* lc =  CLSceneLayoutManager::instance().createRecorderContent(id);
		m_camLayout.getContent()->addLayout(lc, false);
		m_camLayout.addDevice(id, true);
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

bool GraphicsView::onUserInput(bool go_unsteady, bool escapeFromintro)
{
    mSteadyShow.onUserInput(go_unsteady);

    if (escapeFromintro && m_camLayout.getContent() == CLSceneLayoutManager::instance().introScreenLayoutContent())
    {
        emit onIntroScreenEscape();
        return true;
    }

    return false;

}

void GraphicsView::goToSteadyMode(bool steady)
{
    CLUnMovedPixture* bk_item = static_cast<CLUnMovedPixture*>(staticItemByName("background"));

    if (steady)
    {

        foreach(CLAbstractUnmovedItem* item, m_staticItems)
        {

            if (item != bk_item && item->preferNonSteadyMode())
            {
                onUserInput(false, false);
                return;
            }
                
        }


        if ((m_seachItem && m_seachItem->hasFocus()) || m_menuIsHere)
        {
            onUserInput(false, false);
            return;
        }

        foreach(CLAbstractUnmovedItem* item, m_staticItems)
        {
            if (item != bk_item)
                item->hideIfNeeded(500);
        }

        if (m_seachItem && m_seachItem->isVisible())
            m_seachItem->setVisible(false);

        if (m_selectedWnd && m_selectedWnd->isFullScreen())
        {
            m_selectedWnd->goToSteadyMode(true, false);
        }

        

    }
    else
    {
        foreach(CLAbstractUnmovedItem* item, m_staticItems)
        {
            if (item != bk_item)
                item->show(500);
        }

        if (m_seachItem && !m_seachItem->isVisible())
        {
            m_seachItem->setVisible(true);
        }

        if (m_seachItem)
            m_seachItem->setFocus();

        if(m_selectedWnd && m_selectedWnd->isFullScreen())
        {
            m_selectedWnd->goToSteadyMode(false, false);
        }

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

		m_gridItem->hide(global_grid_aparence_delay);
		break;

	}

}

void GraphicsView::keyPressEvent( QKeyEvent * e )
{
	if (!mViewStarted)
		return;

    if (onUserInput(true, true))
        return;

	CLAbstractSceneItem* last_sel_item = getLastSelectedItem();

	//===transform=========
	switch (e->key()) 
	{
		case Qt::Key_S:
			global_show_item_text=!global_show_item_text;
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

			if (m_camLayout.getContent()->checkIntereactionFlag(LayoutContent::GridEnable))
				m_gridItem->show(global_grid_aparence_delay);

			break;

		case Qt::Key_X:
            m_show.m_counrer+=10*60*60;
			break;

        case Qt::Key_Escape:
            if (!mMainWnd->isFullScreen())
                mMainWnd->showFullScreen();
            else
                mMainWnd->showMaximized();
            break;

        case Qt::Key_A:
                onArrange_helper();
            break;



	}

	// ===========new item selection 
	if (e->nativeModifiers() && last_sel_item)
	{

		CLAbstractSceneItem* next_item = 0;

		switch (e->key()) 
		{
		case Qt::Key_Left:
			next_item = m_camLayout.getGridEngine().getNextLeftItem(last_sel_item);
			break;
		case Qt::Key_Right:
			next_item = m_camLayout.getGridEngine().getNextRightItem(last_sel_item);
			break;
		case Qt::Key_Up:
			next_item = m_camLayout.getGridEngine().getNextTopItem(last_sel_item);
			break;
		case Qt::Key_Down:
			next_item = m_camLayout.getGridEngine().getNextBottomItem(last_sel_item);
			break;

		}

        if (m_camLayout.getGridEngine().getSettings().max_rows==1 && next_item && m_selectedWnd!=0 && m_selectedWnd == last_sel_item && m_selectedWnd->isFullScreen())
        {
            // if single raw mode and we are looking at the full scree window
            setZeroSelection();
            onItemFullScreen_helper(next_item, 800);

            if (next_item->getType() == CLAbstractSceneItem::VIDEO)
            {
                CLVideoWindowItem* video_wnd = static_cast<CLVideoWindowItem*>(next_item);
                CLVideoCamera* cam = video_wnd->getVideoCam();
                CLDevice* dev = cam->getDevice();
                if (dev->checkDeviceTypeFlag(CLDevice::RECORDED) || dev->checkDeviceTypeFlag(CLDevice::ARCHIVE))
                {
                    CLAbstractArchiveReader* asr = static_cast<CLAbstractArchiveReader*>(cam->getStreamreader());
                    asr->jumpTo(0, true);
                    cam->streamJump(0);
                }
            }


            //if (next_item)
        }
        else if (next_item)
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

bool GraphicsView::isALTPressed(const QInputEvent* event) const
{
	return event->modifiers() & Qt::AltModifier;
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
    // Initialize maxTextureSize value. It should be somewhere where GL context already initialized, otherwise it will return 0.
    // Probably Medved6 knows better place.
    CLGLRenderer::getMaxTextureSize();

    if (m_camLayout.getContent() == CLSceneLayoutManager::instance().introScreenLayoutContent())// ||
        //m_camLayout.getContent() == CLSceneLayoutManager::instance().startScreenLayoutContent() )
        return; // do not draw bgrd in case of intro video

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

void GraphicsView::updatePageSelector()
{
    if (m_pageSelector)
    {
        int viewPoertWidth = viewport()->width();
        int itemWidth = m_pageSelector->boundingRect().width();
        int itemHeight = m_pageSelector->boundingRect().height();

        m_pageSelector->setStaticPos(QPoint( (viewPoertWidth -  itemWidth) / 2 , viewport()->height() - itemHeight - 15 ));
    }
}

void GraphicsView::updateDecorations()
{
	CLUnMovedPixture* item = static_cast<CLUnMovedPixture*>(staticItemByName("background"));

    if (item)
	{
		item->setMaxSize(viewport()->width(), viewport()->height());
		QSize size = item->getSize();
		item->setStaticPos( QPoint( (viewport()->width() - size.width())/2, (viewport()->height() - size.height())/2 ) );
	}

	item = static_cast<CLUnMovedPixture*>(staticItemByName(button_magnifyingglass));
	if (item)
		item->setStaticPos(QPoint((viewport()->width() - decoration_size)/2,0));

	item = static_cast<CLUnMovedPixture*>(staticItemByName(button_squarelayout));
	if (item)
		item->setStaticPos(QPoint(viewport()->width() - 3.3*decoration_size,1));

	item = static_cast<CLUnMovedPixture*>(staticItemByName(button_longlayout));
	if (item)
		item->setStaticPos(QPoint(viewport()->width() - 2.2*decoration_size,0));

    item = static_cast<CLUnMovedPixture*>(staticItemByName(button_singleLineLayout));
    if (item)
        item->setStaticPos(QPoint(viewport()->width() - 1.1*decoration_size,0));

    updatePageSelector();

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
    Q_UNUSED(event);

	if (!mViewStarted)
		return;

	updateDecorations();
	recalcSomeParams();

	if (m_selectedWnd && m_selectedWnd->isFullScreen())
		onItemFullScreen_helper(m_selectedWnd, 800);
	else
		//fitInView(getRealSceneRect(),Qt::KeepAspectRatio);
	{

		centerOn(getRealSceneRect().center());
		fitInView(1000, 0);
	}


    QList<CLAbstractSceneItem*> lst = m_camLayout.getItemList();
	foreach(CLAbstractSceneItem* item, lst)
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

	return m_camLayout.getGridEngine().getCenterWnd();

}

void GraphicsView::onDecorationItemPressed(QString name)
{

	if (name == button_squarelayout)
	{
		m_camLayout.getGridEngine().getSettings().optimal_ratio = square_ratio;
		m_camLayout.getGridEngine().getSettings().max_rows = 5;
		onArrange_helper();
	}
	else if (name == button_longlayout)
	{
		m_camLayout.getGridEngine().getSettings().optimal_ratio = long_ratio;
		m_camLayout.getGridEngine().getSettings().max_rows = 2;
		onArrange_helper();
	}
    else if (name == button_singleLineLayout)
    {
        m_camLayout.getGridEngine().getSettings().optimal_ratio = long_ratio;
        m_camLayout.getGridEngine().getSettings().max_rows = 1;
        onArrange_helper();
    }
	else
		emit onDecorationPressed(m_camLayout.getContent(), name);
}

void GraphicsView::onScneZoomFinished()
{
    mWheelZooming = false;
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
		onItemFullScreen_helper(wnd, 800);
	else
	{
		// escape FS MODE
		setZeroSelection();
		wnd->setFullScreen(false);
		fitInView(1000, 0, SLOW_START_SLOW_END);

	}
}

void GraphicsView::onNewItemSelected_helper(CLAbstractSceneItem* new_wnd, int delay)
{

	if (!m_camLayout.getContent()->checkIntereactionFlag(LayoutContent::ItemSelectable))
		return;

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

	m_movement.move(point, item_select_duration, delay, SLOW_START_SLOW_END);

	//===================	
	// width 1900 => zoom 0.278 => scale 0.07728
	int width = viewport()->width();
	qreal scale = 0.07728*width/1900.0;
	qreal zoom = m_scenezoom.scaleTozoom(scale) ;

	m_scenezoom.zoom_abs(zoom, item_select_duration, delay, QPoint(0,0), SLOW_START_SLOW_END);

}

void GraphicsView::onCircle_helper(bool show)
{
	if (!mViewStarted)
		return;

	QList<CLAbstractSceneItem*> wndlst = m_camLayout.getItemList();
	if (wndlst.empty())
		return;

	CLParallelAnimationGroup* groupAnimation = new CLParallelAnimationGroup;

	qreal total = wndlst.size();
	if (total<2) 
		return;

	m_camLayout.updateSceneRect();
	QRectF item_rect = m_camLayout.getGridEngine().getGridRect();

    m_show.setCenterPoint(item_rect.center());
    m_show.setRadius(qMax(item_rect.width(), item_rect.height())/8);
    m_show.setItems(m_camLayout.getItemListPointer());

	int i = 0;

	foreach (CLAbstractSceneItem* item, wndlst)
	{
		QPropertyAnimation *anim = AnimationManager::instance().addAnimation(item, "rotation");

		anim->setStartValue(item->getRotation());
		anim->setEndValue(cl_get_random_val(0, 30));

		anim->setDuration(1500 + cl_get_random_val(0, 300));

		anim->setEasingCurve(QEasingCurve::InOutBack);

		groupAnimation->addAnimation(anim);

		//=========================

		QPropertyAnimation *anim2 = AnimationManager::instance().addAnimation(item, "pos");

		anim2->setStartValue(item->pos());

        anim2->setEndValue( QPointF ( m_show.getCenter().x() + cos((i / total) * 6.28) * m_show.getRadius(), m_show.getCenter().y() + sin((i / total) * 6.28) * m_show.getRadius() ) );

		anim2->setDuration(1500 + cl_get_random_val(0, 300));
		anim2->setEasingCurve(QEasingCurve::InOutBack);

		groupAnimation->addAnimation(anim2);

		item->setArranged(false);

		++i;
	}

	if (show)
        connect(groupAnimation, SIGNAL(finished ()), &m_show, SLOT(start()));

	connect(groupAnimation, SIGNAL(finished ()), this, SLOT(fitInView()));

	groupAnimation->start();
	groupAnimation->setDeleteAfterFinished(true);
	m_animationManager.registerAnimation(groupAnimation);

}

void GraphicsView::instantArrange()
{
	QList<CLIdealItemPos> newPosLst = m_camLayout.getGridEngine().calcArrangedPos();
	if (newPosLst.empty())
		return;

	foreach(CLIdealItemPos ipos, newPosLst)
	{
		ipos.item->setRotation(0);
		ipos.item->setPos(ipos.pos);

	}

}

void GraphicsView::onArrange_helper()
{
	if (!mViewStarted)
		return;

	QList<CLAbstractSceneItem*> itemlist = m_camLayout.getItemList();
	if (itemlist.empty())
		return;

	CLParallelAnimationGroup* groupAnimation = new CLParallelAnimationGroup;

	foreach (CLAbstractSceneItem* item, itemlist)
	{
		item->stop_animation();

		QPropertyAnimation *anim1 = AnimationManager::instance().addAnimation(item, "rotation");

		anim1->setStartValue(item->getRotation());
		anim1->setEndValue(0);

		anim1->setDuration(1000 + cl_get_random_val(0, 300));
		anim1->setEasingCurve(QEasingCurve::InOutBack);

		groupAnimation->addAnimation(anim1);

		//=============

	}
	/**/

	QList<CLIdealItemPos> newPosLst = m_camLayout.getGridEngine().calcArrangedPos();
	foreach(CLIdealItemPos ipos, newPosLst)
	{
		CLAbstractSceneItem* item = ipos.item;

		QPropertyAnimation *anim2 = AnimationManager::instance().addAnimation(item, "pos");

		anim2->setStartValue(item->pos());

		anim2->setEndValue(ipos.pos);

		anim2->setDuration(1000 + cl_get_random_val(0, 300));
		anim2->setEasingCurve(QEasingCurve::InOutBack);

		groupAnimation->addAnimation(anim2);

		item->setArranged(false);

	}

	connect(groupAnimation, SIGNAL(finished ()), this, SLOT(fitInView()));
	connect(groupAnimation, SIGNAL(finished ()), this, SLOT(onArrange_helper_finished()));

	groupAnimation->start();
	groupAnimation->setDeleteAfterFinished(true);
	m_animationManager.registerAnimation(groupAnimation);

}

void GraphicsView::onArrange_helper_finished()
{
	QList<CLIdealItemPos> newPosLst = m_camLayout.getGridEngine().calcArrangedPos();
	foreach(CLIdealItemPos ipos, newPosLst)
	{
		CLAbstractSceneItem* item = ipos.item;
		item->setArranged(true);
	}

}

QString getRecordName()
{
    QString base = getTempRecordingDir() + "video%1.ts";
    for (int i = 0; ; i++) {
        QString filePath = base.arg(i == 0 ? QString() : QLatin1String(" ") + QString::number(i));
        if (!QFileInfo(filePath).exists()) {
            return filePath;
        }
    }
}

void GraphicsView::toggleRecording()
{
    bool recording = cm_start_video_recording.property("recoding").toBool();

    VideoRecorderSettings recorderSettings;
   
    if (!recording) {
        cm_start_video_recording.setProperty("recoding", true);

        QAudioDeviceInfo audioDevice = recorderSettings.primaryAudioDevice();
        QAudioDeviceInfo secondAudioDevice = recorderSettings.secondaryAudioDevice();
        int screen = recorderSettings.screen();
        VideoRecorderSettings::CaptureMode captureMode = recorderSettings.captureMode();
        VideoRecorderSettings::DecoderQuality decoderQuality = recorderSettings.decoderQuality();
        VideoRecorderSettings::Resolution resolution = recorderSettings.resolution();
        bool captureCursor = recorderSettings.captureCursor();

        QString filePath = getRecordName();
#ifdef Q_OS_WIN
        if (m_desktopEncoder)
            delete m_desktopEncoder;
        QSize encodingSize(0,0);
        if (resolution == VideoRecorderSettings::ResQuaterNative)
            encodingSize = QSize(-2, -2);
        else if (resolution == VideoRecorderSettings::Res1920x1080)
            encodingSize = QSize(1920, 1080);
        else if (resolution == VideoRecorderSettings::Res1280x720)
            encodingSize = QSize(1280, 720);
        else if (resolution == VideoRecorderSettings::Res640x480)
            encodingSize = QSize(640, 480);
        
        float quality = 1.0;
        if (decoderQuality == VideoRecorderSettings::BalancedQuality)
            quality = 0.75;
        else if (decoderQuality == VideoRecorderSettings::PerformanceQuality)
            quality = 0.5;
        
        CLScreenGrapper::CaptureMode grabberCaptureMode = CLScreenGrapper::CaptureMode_Application;
        if (captureMode == VideoRecorderSettings::FullScreenMode)
            grabberCaptureMode = CLScreenGrapper::CaptureMode_DesktopWithAero;
        else if (captureMode == VideoRecorderSettings::FullScreenNoeroMode)
            grabberCaptureMode = CLScreenGrapper::CaptureMode_DesktopWithoutAero;

        m_desktopEncoder = new DesktopFileEncoder(filePath, screen, &audioDevice, &secondAudioDevice, grabberCaptureMode, captureCursor, encodingSize, quality, viewport());
#endif
        QLabel *label = new QLabel;
        label->move(width()/2 - 100, 300);
        label->resize(200, 100);
        label->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        label->setText(tr("Recording started"));
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("QLabel { color:white; background:black }");
        label->setFocusPolicy(Qt::NoFocus);
        label->show();
        QPropertyAnimation *animation = new QPropertyAnimation(label, "windowOpacity", label);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->setDuration(3000);
        animation->setStartValue(1.0);
        animation->setEndValue(0.0);
        animation->start();
        QTimer::singleShot(3000, label, SLOT(deleteLater()));
    } else 
    {
        // stop capturing
        cm_start_video_recording.setProperty("recoding", QVariant());

#ifdef Q_OS_WIN
        QString recordedFileName = m_desktopEncoder->fileName();
        m_desktopEncoder->stop();

        QSettings settings;
        settings.beginGroup("videoRecording");
        QString previousFile = settings.value(QLatin1String("previousFile")).toString();
        previousFile = QFileInfo(previousFile).path() +
                QDir::separator() +
                QFileInfo(recordedFileName).baseName();

        QString filePath = QFileDialog::getSaveFileName(this, tr("Save Recording As"), previousFile, tr("Transport Stream (*.ts)"));

		delete m_desktopEncoder;
        m_desktopEncoder = 0;

        if (!filePath.isEmpty()) {
            QFile::remove(filePath);
            bool result = QFile::rename(recordedFileName, filePath);
            if (!result) {
                // handle error
                QFile::remove(recordedFileName);
            }

            settings.setValue(QLatin1String("previousFile"), previousFile);
        } else {
            QFile::remove(recordedFileName);
        }
        settings.endGroup();
#endif
    }
}

void GraphicsView::recordingSettings()
{
//    VideoRecordingDialog dialog;
//    ArthurStyle style;
//    dialog.setStyle(&style);

//    dialog.exec();
    PreferencesWindow preferencesDialog;
    preferencesDialog.setCurrentTab(3);
    preferencesDialog.exec();
}

void GraphicsView::fitInView(int duration, int delay, CLAnimationCurve curve)
{

	if (m_selectedWnd && isItemStillExists(m_selectedWnd) && m_selectedWnd->isFullScreen())
	{
		m_selectedWnd->setFullScreen(false);
		m_selectedWnd->zoom_abs(1.0, 0, 0);
		update();
	}	/**/

	m_camLayout.updateSceneRect();
	QRectF item_rect = m_camLayout.getGridEngine().getGridRect();

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

	m_scenezoom.zoom_abs(zoom, duration, delay, QPoint(0,0), curve);

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

void GraphicsView::onItemFullScreen_helper(CLAbstractSceneItem* wnd, int duration)
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

	
	m_movement.move(item_rect.center(), duration, 0, SLOW_START_SLOW_END);

	//scale(scl, scl);

	m_scenezoom.zoom_abs(zoom, duration, 0, QPoint(0,0), SLOW_START_SLOW_END);

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
			m_camLayout.addLayoutItem(name, lc, true);
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
				m_camLayout.addLayoutItem(name, lc, true);
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

	LayoutContent* content_copy = LayoutContent::coppyLayoutContent(content);

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

bool GraphicsView::contextMenuHelper_existRecordedVideo(CLVideoCamera* cam)
{
    QString id = getTempRecordingDir() + cam->getDevice()->getUniqueId();
    return QDir(id).exists();
}

void GraphicsView::contextMenuHelper_viewRecordedVideo(CLVideoCamera* cam)
{
	cam->stopRecording();

    QString id = getTempRecordingDir() + cam->getDevice()->getUniqueId();

	m_camLayout.addDevice(id, true);
    m_camLayout.getContent()->addDevice(id);
	fitInView(600, 100, SLOW_START_SLOW_END);
}

void GraphicsView::contextMenuHelper_saveRecordedAs(CLVideoCamera* cam)
{
    if (!cam->getDevice()->checkDeviceTypeFlag(CLDevice::RECORDED))
        return;

    QString suggetion = (static_cast<CLArchiveDevice*>(cam->getDevice()))->originalName();

    bool ok;
    QString name;
    while(true)
    {
        name = UIgetText(this, tr("Save recorded video as"), tr("Title:"), suggetion, ok).trimmed();
        if (!ok)
            break;

        if (name.isEmpty())
        {
            UIOKMessage(this, "", "Please provide a title.");
            continue;
        }

        // make sure we'll be able to create such folder
        name.replace( QRegExp(
             "[^"
                "A-Z,a-z,0-9,"
                "\\^,\\&,\\',\\@,"
                "\\{,\\},\\[,\\],"
                "\\,,\\$,\\=,\\!,"
                "\\-,\\#,\\(,\\),"
                "\\%,\\.,\\+,\\~,\\_"
             "]"), "_" );


        QDir dir(getRecordingDir() + name);
        if (dir.exists())
        {
            UIOKMessage(this, "", "Appears this title already exists.");
            continue;
        }
        
        if (!dir.mkpath(getRecordingDir() + name))
        {
            UIOKMessage(this, "", "Can't save with this title. Please provide another one.");
            continue;
        }

        dir.rmdir(getRecordingDir() + name);
        break;
    }

    if (ok)
    {
        CLArchiveStreamReader* rreader = static_cast<CLArchiveStreamReader*>(cam->getStreamreader());
        rreader->setRecordedDataDst(name);
    }


}

void GraphicsView::contextMenuHelper_openInWebBroser(CLVideoCamera* cam)
{
	CLNetworkDevice* net_device = static_cast<CLNetworkDevice*>(cam->getDevice());

	QString url;
	QTextStream(&url) << "http://" << net_device->getIP().toString();

	CLWebItem* wi = new CLWebItem(this, m_camLayout.getGridEngine().calcDefaultMaxItemSize().width(), m_camLayout.getGridEngine().calcDefaultMaxItemSize().height());
	m_camLayout.addItem(wi, true);
	wi->navigate(url);

	fitInView(600, 100, SLOW_START_SLOW_END);

}

void GraphicsView::contextMenuHelper_Rotation(CLAbstractSceneItem* wnd, qreal angle)
{
	QRectF view_scene = mapToScene(viewport()->rect()).boundingRect(); //viewport in the scene cord
	QRectF view_item = wnd->mapFromScene(view_scene).boundingRect(); //viewport in the item cord
	QPointF center_point_item = wnd->boundingRect().intersected(view_item).center(); // center of the intersection of the vewport and item
	wnd->setRotationPointCenter(center_point_item);

	wnd->z_rotate_abs(center_point_item, angle, 600, 0);

}

void GraphicsView::contextMenuHelper_saveLayout( bool new_name)
{
    if (new_name)
    {
        bool ok;
        QString name;
        while(true)
        {
            name = UIgetText(this, tr("Save layout as"), tr("Layout title:"), "", ok).trimmed();
            if (!ok)
                break;

            if (name.isEmpty())
            {
                UIOKMessage(this, "", "Empty tittle cannot be used.");
                continue;
            }

            name = QString("Layout:") + name;

            if (!CLSceneLayoutManager::instance().getAllLayoutsContent()->hasSuchSublayoutName(name))
                break;

            //YesNoCancel(this, )

            UIOKMessage(this, "", "Such title layout already exists.");

        }

        if (!ok)
            return;

        m_camLayout.getGridEngine().clarifyLayoutContent();
        LayoutContent* new_cont = LayoutContent::coppyLayoutContent(m_camLayout.getContent());

        new_cont->addDecorationFlag(LayoutContent::MagnifyingGlass);
        new_cont->removeDecorationFlag(LayoutContent::SearchEdit);

        new_cont->setName(name);
        CLDeviceCriteria cr(CLDeviceCriteria::STATIC);
        new_cont->setDeviceCriteria(cr);
        CLSceneLayoutManager::instance().getAllLayoutsContent()->addLayout(new_cont, false);

    }
    else
        m_camLayout.saveLayoutContent();

}

void GraphicsView::navigation_grid_items_drop_helper()
{
	bool first_animation = true;
	CLParallelAnimationGroup*  groupAnimation;

	CLGridEngine& ge = m_camLayout.getGridEngine();

    QList<QGraphicsItem *> lst = m_scene.selectedItems();

	foreach(QGraphicsItem* itm, lst)
	{
		CLAbstractSceneItem* item = navigationItem(itm);
        if (!item)
            continue;

		item->setCanDrop(false);

		CLAbstractSceneItem* item_to_swap_with = m_camLayout.getGridEngine().getItemToSwapWith(item);

		if (first_animation)
		{
			stopAnimation();
			groupAnimation = new CLParallelAnimationGroup;
			first_animation = false;
		}

		if (item_to_swap_with) // if we have smoothing to swap with
		{
			int original_slot_x, original_slot_y;
			ge.slotFromPos(item->getOriginalPos().toPoint(), original_slot_x, original_slot_y);
			QPointF item_to_swap_with_newPos = ge.adjustedPosForSlot(item_to_swap_with, original_slot_x, original_slot_y);

			int new_slot_x, new_slot_y;
			ge.slotFromPos(item_to_swap_with->scenePos().toPoint(), new_slot_x, new_slot_y);
			QPointF item_newPos = ge.adjustedPosForSlot(item, new_slot_x, new_slot_y);

			QPropertyAnimation *anim = AnimationManager::instance().addAnimation(item, "pos");
			anim->setStartValue(item->pos());
			anim->setEndValue(item_newPos);
			anim->setDuration(1000 + cl_get_random_val(0, 300));
			anim->setEasingCurve(QEasingCurve::InOutBack);
			groupAnimation->addAnimation(anim);
			item->setArranged(false);

			item_to_swap_with->setZValue(global_base_scene_z_level + 1); // this item 
			anim = AnimationManager::instance().addAnimation(item_to_swap_with, "pos");
			anim->setStartValue(item_to_swap_with->pos());
			anim->setEndValue(item_to_swap_with_newPos);
			anim->setDuration(1000 + cl_get_random_val(0, 300));
			anim->setEasingCurve(QEasingCurve::InOutBack);
			groupAnimation->addAnimation(anim);
			item_to_swap_with->setArranged(false);
		}
		else if (ge.canBeDropedHere(item)) // just adjust the item 
		{
			QPropertyAnimation *anim = AnimationManager::instance().addAnimation(item, "pos");
			anim->setStartValue(item->pos());
			anim->setEndValue(ge.adjustedPosForItem(item));
			anim->setDuration(1000 + cl_get_random_val(0, 300));
			anim->setEasingCurve(QEasingCurve::InOutBack);
			groupAnimation->addAnimation(anim);
			item->setArranged(false);
		}
		else // item should be moved to original position, and adjust it 
		{
			int original_slot_x, original_slot_y;
			ge.slotFromPos(item->getOriginalPos().toPoint(), original_slot_x, original_slot_y);

			QPropertyAnimation *anim = AnimationManager::instance().addAnimation(item, "pos");
			anim->setStartValue(item->pos());
			anim->setEndValue(ge.adjustedPosForSlot(item, original_slot_x, original_slot_y));
			anim->setDuration(1000 + cl_get_random_val(0, 300));
			anim->setEasingCurve(QEasingCurve::InOutBack);
			groupAnimation->addAnimation(anim);
			item->setArranged(false);
		}

	}

	m_scene.clearSelection();

	if (!first_animation)
	{
		groupAnimation->setDeleteAfterFinished(true);
		groupAnimation->start();
		connect(groupAnimation, SIGNAL(finished()), this, SLOT(on_grid_drop_animation_finished()));
		m_animationManager.registerAnimation(groupAnimation);
	}

}

void GraphicsView::on_grid_drop_animation_finished()
{
    QList<CLAbstractSceneItem*> lst = m_camLayout.getItemList();
	
    foreach(CLAbstractSceneItem* itm, lst)
	{
		itm->setZValue(global_base_scene_z_level);
		itm->setArranged(true);
	}
	m_camLayout.updateSceneRect();
}

void GraphicsView::contextMenuHelper_restoreLayout()
{
    emit onNewLayoutSelected(0, m_camLayout.getContent());
}
