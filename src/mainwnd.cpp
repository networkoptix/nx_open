#include "mainwnd.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QByteArray>
#include "base/log.h"
#include "base/rand.h"
#include <QMessageBox>
#include <QTimer>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include "base/sleep.h"
#include <QMenu>
#include <QGLWidget>
#include "device/asynch_seacher.h"
#include "camera/camera.h"
#include "streamreader/streamreader.h"
#include "video_camera.h"
#include "device/directory_browser.h"

#include <QTransform>
#include "../src/gui/kernel/qevent.h"
#include "ui/videoitem/video_wnd_item.h"
#include "ui/video_cam_layout/start_screen_content.h"

QColor bkr_color(0,5,5,125);
//QColor bkr_color(9/1.5,54/1.5,81/1.5);



MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags):
//QMainWindow(parent, flags),
m_videoView(this)
{
	//ui.setupUi(this);
	setWindowTitle("HDWitness");

	QVBoxLayout *mainLayout = new QVBoxLayout;
	
	//=======================================================

	//setWindowOpacity(.80);

	
	
	m_videoView.setCamLayOut(&m_camlayout);
	m_videoView.setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers))); //Antialiasing
	//m_videoView.setViewport(new QGLWidget());
	m_videoView.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform	| QPainter::TextAntialiasing); //Antialiasing
	setAutoFillBackground(false);
	//m_videoView.viewport()->setAutoFillBackground(false);


	//m_videoView.setSceneRect(0,0,2*SCENE_LEFT, 2*SCENE_TOP);

	m_videoView.setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
	//m_videoView.setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
	//m_videoView.setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);


	//m_videoView.setCacheMode(QGraphicsView::CacheBackground);// slows down scene drawing a lot!!!


	//m_videoView.setDragMode(QGraphicsView::ScrollHandDrag);

	m_videoView.setOptimizationFlag(QGraphicsView::DontClipPainter);
	m_videoView.setOptimizationFlag(QGraphicsView::DontSavePainterState);
	m_videoView.setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing); // Antialiasing?

	m_videoView.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_videoView.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	//m_videoView.setBackgroundBrush(QPixmap("c:/photo/32.jpg"));
	//m_scene.setItemIndexMethod(QGraphicsScene::NoIndex);

	m_videoView.setScene(&m_scene);
	

	QPalette palette;
	palette.setColor(backgroundRole(), bkr_color);
	setPalette(palette);
	



	palette.setColor(m_videoView.backgroundRole(), bkr_color);
	m_videoView.setPalette(palette);

	/*
	QRadialGradient radialGrad(500, 0, 1000);
	radialGrad.setColorAt(0, Qt::red);
	radialGrad.setColorAt(0.5, Qt::blue);
	radialGrad.setColorAt(1, Qt::green);
	m_videoView.setBackgroundBrush(radialGrad);
	/**/

	//=======================================================

	mainLayout->addWidget(&m_videoView);
	setLayout(mainLayout);

	//=======================================================


	//QThread::currentThread()->setPriority(QThread::HighPriority); // Priority more tnan decoders have

	const int min_wisth = 1200;
	setMinimumWidth(min_wisth);
	setMinimumHeight(min_wisth*3/4);


	
	//m_videoView.setTransformationAnchor(QGraphicsView::NoAnchor);
	//m_videoView.setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	//m_videoView.setResizeAnchor(QGraphicsView::NoAnchor);
	//m_videoView.setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

	//m_videoView.setAlignment(Qt::AlignVCenter);
	
	//showFullScreen();

	m_camlayout.setEventHandler(this);
	m_camlayout.setView(&m_videoView);
	m_camlayout.setScene(&m_scene);


	m_camlayout.setContent(startscreen_content());

	m_camlayout.start();
}



MainWnd::~MainWnd()
{
	CLDeviceManager::instance().getDiveceSercher().wait();
	m_camlayout.stop();
}

void MainWnd::closeEvent ( QCloseEvent * event )
{
	m_videoView.closeAllDlg();
}


void MainWnd::toggleFullScreen()
{
	if (!isFullScreen())
		showFullScreen();
	else
		showMaximized();
}


//====================================================================
//====================================================================

