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
#include "uvideo_wnd.h"
#include "video_camera.h"
#include "device/directory_browser.h"

#include <QTransform>

QColor bkr_color(0,5,15);

#define CL_VIDEO_ROWS 3

#define SLOT_WIDTH 640
#define SLOT_HEIGHT (SLOT_WIDTH*3/4)
#define SLOT_DISTANCE (0.3)


extern CLDiviceSeracher dev_searcher;


MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags):
//QMainWindow(parent, flags),
m_selectedcCam(0),
m_camlayout(CL_VIDEO_ROWS, CL_MAX_CAM_SUPPORTED)
{
	//ui.setupUi(this);


	QVBoxLayout *mainLayout = new QVBoxLayout;
	QPalette palette;
	//=======================================================

	setAutoFillBackground(false);

	
	
	
	m_videoView.setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers))); //Antialiasing
	//m_videoView.setViewport(new QGLWidget());
	m_videoView.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform	| QPainter::TextAntialiasing); //Antialiasing


	m_videoView.setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
	//m_videoView.setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
	//m_videoView.setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);


	//m_videoView.setCacheMode(QGraphicsView::CacheBackground);


	//m_videoView.setDragMode(QGraphicsView::ScrollHandDrag);

	m_videoView.setOptimizationFlag(QGraphicsView::DontClipPainter);
	m_videoView.setOptimizationFlag(QGraphicsView::DontSavePainterState);
	m_videoView.setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing); // Antialiasing?

	//m_videoView.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	//m_videoView.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	m_videoView.setScene(&m_scene);

	palette.setColor(backgroundRole(), bkr_color);
	setPalette(palette);


	palette.setColor(m_videoView.backgroundRole(), bkr_color);
	m_videoView.setPalette(palette);

	//=======================================================

	mainLayout->addWidget(&m_videoView);
	setLayout(mainLayout);

	//=======================================================

	m_timer = new QTimer(this);
	connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
	m_timer->start(100); // first time should come fast


	m_videotimer = new QTimer(this);
	connect(m_videotimer, SIGNAL(timeout()), this, SLOT(onVideoTimer()));
	m_videotimer->start(1000/40); 


	QThread::currentThread()->setPriority(QThread::HighPriority); // Priority more tnan decoders have

	const int min_wisth = 1200;
	setMinimumWidth(min_wisth);
	setMinimumHeight(min_wisth*3/4);

	/**/
	QPen pen;
	pen.setBrush(Qt::green);
	pen.setColor(Qt::green);

	int x_tar = 4000;
	int y_tar = 1000;

	QGraphicsItem* rect = m_scene.addLine(0,0,2000,0, pen);
	rect->setPos(x_tar - 1000,y_tar);

	rect = m_scene.addLine(0,0,0,2000, pen);

	rect->setPos(x_tar,0);

	/**/

	//m_videoView.setSceneRect(0,0, 10000,10000);

	//m_videoView.translate(2000,1000);

	m_videoView.setTransformationAnchor(QGraphicsView::NoAnchor);
	m_videoView.setResizeAnchor(QGraphicsView::NoAnchor);
	m_videoView.setAlignment(Qt::AlignVCenter);
	m_videoView.setAlignment(0);
	//m_videoView.centerOn(4000,1000);



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


void MainWnd::onTimer()
{
	static const int interval = 1000;
	static bool first_time = true;
	if (first_time)
	{
		dev_searcher.start();
		

		CLDirectoryBrowserDeviceServer dirbrowsr("c:/Photo");
		onNewDevices_helper(dirbrowsr.findDevices());
		

		first_time = false;
	}

	if (!dev_searcher.isRunning() )
	{
		// at least we once found smth => can increase interval
		if (m_timer->interval()!=interval)
		{
			//m_timer->stop();
			m_timer->setInterval(interval);

		}

		
		onNewDevices_helper(dev_searcher.result());

		dev_searcher.start(); // run searcher again ...
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

	int position = m_camlayout.getNextAvailablePos(device->getVideoLayout());
	if (position==-1)
	{
		// max reached
		cl_log.log("Cannot support so many devicees ", cl_logWARNING);
		//delete device; //here+
		device->releaseRef();
		return;
	}

	int width_max, heigh_max;
	getMaxWndSize(device->getVideoLayout(), width_max, heigh_max);

	VideoWindow* video_wnd =  new VideoWindow(device->getVideoLayout(), width_max, heigh_max);
	CLVideoCamera* cam = new VideoCamera(device, video_wnd);

	m_camlayout.addCamera(position, cam);


	if (device->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT)) // do not show fps if it's still shot
		video_wnd->showFPS(false);



	video_wnd->setInfoText(cam->getDevice()->toString());

	connect(video_wnd, SIGNAL(onVideoItemSelected(CLVideoWindow*)), this, SLOT(onVideoItemSelected(CLVideoWindow*)));
	connect(video_wnd, SIGNAL(onVideoItemMouseRightClick(CLVideoWindow*)), this, SLOT(onVideoItemMouseRightClick(CLVideoWindow*)));
	connect(video_wnd, SIGNAL(onAspectRatioChanged(CLVideoWindow*)), this, SLOT(onAspectRatioChanged(CLVideoWindow*)));

	

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


	int x,y;

	getCamPosition_helper(cam, position, x, y);
	video_wnd->setPos(x , y );


	m_scene.addItem(video_wnd);
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



	if (pl.exists("resolution")) 
		pl.get("resolution").value.value = "half";

	reader->setStreamParams(pl);

}

//====================================================================
//====================================================================

CLVideoCamera* MainWnd::getCamByWnd(CLVideoWindow* wnd)
{

	for (int i = 0; i < m_cams.size();++i)
	{
		CLVideoCamera* cam = m_cams.at(i);
			if (cam->getVideoWindow() == wnd)
				return cam;
	}

	return 0;

}

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

void MainWnd::onAspectRatioChanged(CLVideoWindow* wnd)
{
	
	CLVideoCamera* cam = getCamByWnd(wnd);
	if (!cam) return;

	int x,y;

	getCamPosition_helper(cam, m_camlayout.getPos(cam), x, y);

	wnd->setPos(x , y );
	

}

void MainWnd::getMaxWndSize(const CLDeviceVideoLayout* layout, int& max_width, int& max_height)
{
	int dlw = layout->width();
	int dlh = layout->height();
	max_width = SLOT_WIDTH*(dlw + SLOT_DISTANCE*(dlw-1));
	max_height = SLOT_HEIGHT*(dlh + SLOT_DISTANCE*(dlh-1));
}

void MainWnd::getCamPosition_helper(CLVideoCamera* cam, int position, int& x_pos, int& y_pos)
{
	CLVideoWindow* wnd = cam->getVideoWindow();


	float aspect = wnd->aspectRatio();


	int width_max, height_max;
	getMaxWndSize(cam->getDevice()->getVideoLayout(), width_max, height_max);


	int width = wnd->width();
	int height = wnd->height();

	int dx = (width_max - width)/2;
	int dy = (height_max - height)/2;

	int x,y;
	m_camlayout.getXY(position, x, y);

	x_pos = x * SLOT_WIDTH* (1+SLOT_DISTANCE) + dx ;
	y_pos = y * SLOT_HEIGHT * (1+SLOT_DISTANCE) + dy ;


}