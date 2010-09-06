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
#include "./ui/videoitem/uvideo_wnd.h"
#include "video_camera.h"
#include "device/directory_browser.h"

#include <QTransform>
#include "../src/gui/kernel/qevent.h"

QColor bkr_color(0,5,5,125);
//QColor bkr_color(9/1.5,54/1.5,81/1.5);

#define CL_VIDEO_ROWS 4





extern CLDiviceSeracher dev_searcher;
extern int scene_zoom_duration;


MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags):
//QMainWindow(parent, flags),
m_selectedcCam(0),
m_camlayout(&m_videoView, &m_scene, CL_VIDEO_ROWS, 40),
m_scene_right(0),
m_scene_bottom(0),
m_videoView(this)
{
	//ui.setupUi(this);
	setWindowTitle("HDWitness");

	QVBoxLayout *mainLayout = new QVBoxLayout;
	
	//=======================================================

	

	
	
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

	m_timer = new QTimer(this);
	connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
	m_timer->start(100); // first time should come fast


	m_videotimer = new QTimer(this);
	connect(m_videotimer, SIGNAL(timeout()), this, SLOT(onVideoTimer()));
	m_videotimer->start(1000/35); 


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
}



MainWnd::~MainWnd()
{
	cl_log.log("Destructor of main window in progress......\r\n ", cl_logDEBUG1);
	
	m_timer->stop();
	delete m_timer;

	m_videotimer->stop();
	delete m_videotimer;

	dev_searcher.wait();

	for (int i = 0; i < m_videoWindows.size();++i)
	{
		VideoWindow* wnd = m_videoWindows.at(i);
		wnd->before_destroy();
	}

	// firs of all lets ask all threads to stop;
	// we do not need to do it sequentially
	for (int i = 0; i < m_cams.size();++i)
	{
		CLVideoCamera* cam = m_cams.at(i);
		cam->getStreamreader()->pleaseStop();
		cam->getCamCamDisplay()->pleaseStop();
	}

	// after we can wait for each thread to stop
	for (int i = 0; i < m_cams.size();++i)
	{
		cl_log.log("About to shut down camera ", i,"\r\n", cl_logDEBUG1);
		CLVideoCamera* cam = m_cams.at(i);
		cam->stopDispay();
		delete cam;
	}

	{
		QMutexLocker lock(&dev_searcher.all_devices_mtx);
		CLDeviceList& list = dev_searcher.getAllDevices();
		CLDevice::deleteDevices(list);
	}

}

void MainWnd::toggleFullScreen()
{
	if (!isFullScreen())
		showFullScreen();
	else
		showMaximized();
}

void MainWnd::mousePressEvent ( QMouseEvent * event)
{
	if (event->button() == Qt::MidButton)
	{
		//toggleFullScreen();
	}

}

void MainWnd::onTimer()
{
	static const int interval = 1000;
	static bool first_time = true;

	if (!dev_searcher.isRunning() )
	{
		onNewDevices_helper(dev_searcher.result());

		dev_searcher.start(); // run searcher again ...
	}

	if (first_time)
	{
		//m_videoView.init();
	}

	if (first_time && m_scene.items().size()==0) 
	{
		CLDirectoryBrowserDeviceServer dirbrowsr("c:/Photo");
		onNewDevices_helper(dirbrowsr.findDevices());
	}

	if (first_time && m_scene.items().size()) // at least something is found
	{

		m_videoView.init();

		// at least we once found smth => can increase interval
		if (m_timer->interval()!=interval)
		{
			//m_timer->stop();
			m_timer->setInterval(interval);

		}


		QThread::currentThread()->setPriority(QThread::HighPriority); // Priority more tnan decoders have

		dev_searcher.start();



		onFirstSceneAppearance();

		first_time = false;
	}




	


}

void MainWnd::onVideoTimer()
{
	for (int i = 0; i < m_videoWindows.size();++i)
	{
		VideoWindow* wnd = m_videoWindows.at(i);
		if (wnd->needUpdate())
		{
			wnd->update();
			wnd->needUpdate(false);
		}
	}

}


void MainWnd::onNewDevice(CLDevice* device)
{
	//return;
	if (!m_camlayout.isSpaceAvalable())
	{
		// max reached
		cl_log.log("Cannot support so many devicees ", cl_logWARNING);
		//delete device; //here+
		device->releaseRef();
		return;
	}

	QSize wnd_size = m_camlayout.getMaxWndSize(device->getVideoLayout());
	

	VideoWindow* video_wnd =  new VideoWindow(&m_videoView, device->getVideoLayout(), wnd_size.width() , wnd_size.height());
	CLVideoCamera* cam = new VideoCamera(device, video_wnd);
	video_wnd->setVideoCam(cam);

	m_camlayout.addWnd(video_wnd);


	if (device->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT)) // do not show fps if it's still shot
	{
		video_wnd->showFPS(false);
		video_wnd->setShowInfoText(false);
		video_wnd->setShowImagesize(false);
	}

	if (device->checkDeviceTypeFlag(CLDevice::ARCHIVE)) // do not show fps if it's still shot
	{
		video_wnd->setShowInfoText(false);
	}




	video_wnd->setInfoText(cam->getDevice()->toString());

	//connect(video_wnd, SIGNAL(onVideoItemSelected(CLVideoWindow*)), this, SLOT(onVideoItemSelected(CLVideoWindow*)));
	//connect(video_wnd, SIGNAL(onVideoItemMouseRightClick(CLVideoWindow*)), this, SLOT(onVideoItemMouseRightClick(CLVideoWindow*)));

	

	m_videoWindows.push_back(video_wnd);
	m_cams.push_back(cam);

	{
		QMutexLocker lock(&dev_searcher.all_devices_mtx);
		dev_searcher.getAllDevices().insert(device->getUniqueId(),device);
	}



	CLStreamreader*  reader = cam->getStreamreader();
	reader->setDeviceRestartHadlerInfo(cam);
	reader->setDeviceRestartHadler(this);

	CLParamList& pl = device->getStreamPramList();
	reader->setStreamParams(pl);

	onDeviceRestarted(reader, cam);


	cam->startDispay();


}



void MainWnd::onNewDevices_helper(CLDeviceList devices)
{
	
	CLDeviceList::iterator it = devices.begin(); 


	while (it!=devices.end())
	{
		CLDevice* device = it.value();

		if (device->getStatus().checkFlag(CLDeviceStatus::ALL) == 0  ) //&&  device->getMAC()=="00-1A-07-00-12-DB")
		{
			device->getStatus().setFlag(CLDeviceStatus::READY);
			onNewDevice(device);


		}
		else
		{
			//delete device; //here+
			device->releaseRef();
		}

		++it;

	}

}

void MainWnd::onFirstSceneAppearance()
{
	m_videoView.centerOn(m_videoView.getRealSceneRect().center());
	m_videoView.zoomDefault(scene_zoom_duration);
}

void MainWnd::onDeviceRestarted(CLStreamreader* reader, CLRestartHadlerInfo info)
{
	// if this is called be reader => reader&cam&device still existing
	CLVideoCamera* cam = reinterpret_cast<CLVideoCamera*>(info);
	CLDevice *device = cam->getDevice();


	CLValue maxSensorWidth;
	CLValue maxSensorHight;
	device->getParam("MaxSensorWidth", maxSensorWidth);
	device->getParam("MaxSensorHeight", maxSensorHight);

	device->setParam_asynch("sensorleft", 0);
	device->setParam_asynch("sensortop", 0);
	device->setParam_asynch("sensorwidth", maxSensorWidth);
	device->setParam_asynch("sensorheight", maxSensorHight);


	CLParamList pl = reader->getStreamParam();//device->getStreamPramList();

	pl.get("streamID").value.value = (int)cl_get_random_val(1, 32000);
	//========this is due to bug in AV firmware;
	// you cannot set up maxSensorWidth with HTTP. ( you can do it in tftp if you are really want to ). 
	// for now(13 December 2009) if we use HTTP => right should be divisible by 64; bottom - by 32
	// may be while you are looking at this comments bug already fixed.
	int right  = pl.get("image_right").value.value;
	right = right/64*64;
	pl.get("image_right").value.value = right;


	int bottom = pl.get("image_bottom").value.value;
	bottom = bottom/32*32;
	pl.get("image_bottom").value.value = bottom;
	//===================

	if (pl.exists("codec")) // if cam supports H.264
		pl.get("codec").value.value = "H.264";// : "JPEG";
		//pl.get("codec").value.value = "JPEG";// : "JPEG";



	if (pl.exists("resolution")) 
		pl.get("resolution").value.value = "half";

	reader->setStreamParams(pl);

}

//====================================================================
//====================================================================

bool MainWnd::isSelectedCamStillExists() const
{
	if (m_selectedcCam==0)
		return false;

	for (int i = 0; i < m_cams.size();++i)
	{
		CLVideoCamera* cam = m_cams.at(i);
		if (cam==m_selectedcCam)
			return true;
	}

	return false;

}

