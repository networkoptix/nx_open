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

#define CL_VIDEO_ROWS 5


extern CLDiviceSeracher dev_searcher;


MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags):
//QMainWindow(parent, flags),
m_selectedcCam(0)
{
	//ui.setupUi(this);

	for (int i = 0; i < CL_MAX_CAM_SUPPORTED; ++i)
		m_position_busy[i] = false;


	QVBoxLayout *mainLayout = new QVBoxLayout;
	QPalette palette;
	//=======================================================

	setAutoFillBackground(false);

	//m_videoView.setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers)));
	m_videoView.setViewport(new QGLWidget());


	m_videoView.setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
	//m_videoView.setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
	//m_videoView.setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);

	m_videoView.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);


	//m_videoView.setCacheMode(QGraphicsView::CacheBackground);


	//m_videoView.setDragMode(QGraphicsView::ScrollHandDrag);

	m_videoView.setOptimizationFlag(QGraphicsView::DontClipPainter);
	m_videoView.setOptimizationFlag(QGraphicsView::DontSavePainterState);
	m_videoView.setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);

	m_videoView.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_videoView.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

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

	int position = next_available_pos();
	if (position==-1)
	{
		// max reached
		cl_log.log("Cannot support so many devicees ", cl_logWARNING);
		//delete device; //here+
		device->releaseRef();
		return;
	}

	VideoWindow* video_wnd =  new VideoWindow(device->getVideoLayout());
	CLVideoCamera* cam = new VideoCamera(device, video_wnd);

	if (device->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT)) // do not show fps if it's still shot
		video_wnd->showFPS(false);



	video_wnd->setInfoText(cam->getDevice()->toString());

	connect(video_wnd, SIGNAL(onVideoItemSelected(CLVideoWindow*)), this, SLOT(onVideoItemSelected(CLVideoWindow*)));
	connect(video_wnd, SIGNAL(onVideoItemMouseRightClick(CLVideoWindow*)), this, SLOT(onVideoItemMouseRightClick(CLVideoWindow*)));


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




	QRectF rect = video_wnd->boundingRect();
	int x,y;

	getXY_by_position(position, CL_VIDEO_ROWS, x, y);
	//video_wnd->setPos(x * rect.width()* 1.1, y * rect.height() * 1.1);
	video_wnd->setPos(x * 640* 1.1, y * 640*3/4 * 1.1);
	video_wnd->setPosition(position);
	m_scene.addItem(video_wnd);
	m_position_busy[position] = true;


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
void MainWnd::getXY_by_position(int position, int unit_size, int&x, int& y)
{

	if (position==0)
	{
		x = 0;
		y = 0;
		return;
	}

	int unitsize2 = unit_size*unit_size;

	int unit_num = position/unitsize2;
	int in_unit_position = position%unitsize2;

	if (unit_num==0) // very first cell
	{
		if (position >= unit_size*(unit_size-1)) // last_row
		{
			y = unit_size-1;
			x = position%unit_size;
			return;
		}

		getXY_by_position(position, unit_size-1 , x,y);

		return;
	}
	else
	{
		x = unit_num*unit_size + in_unit_position/unit_size;
		y = in_unit_position%unit_size;
	}


}

int MainWnd::next_available_pos() const
{
	int index = 0;
	while(1)
	{
		if (index==CL_MAX_CAM_SUPPORTED)
			return -1;

		if (!m_position_busy[index])
			return index;

		++index;
	}

}


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
