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
#include "settings.h"





QString start_screen = "start screen";
QString video_layout = "video layout";

QString button_logo = "logo";
QString button_home = "home";


MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags):
//QMainWindow(parent, flags),
m_videoView(this)
{
	//ui.setupUi(this);
	setWindowTitle("HDWitness");

	setAutoFillBackground(false);

	QPalette palette;
	palette.setColor(backgroundRole(), app_bkr_color);
	setPalette(palette);

	//setWindowOpacity(.80);

	//=======================================================

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(&m_videoView);
	setLayout(mainLayout);

	//=======================================================


	const int min_wisth = 400;
	setMinimumWidth(min_wisth);
	setMinimumHeight(min_wisth*3/4);

	
	//showFullScreen();

	
	m_videoView.getCamLayOut().setEventHandler(this);

	toggleFullScreen();

	connect(&m_videoView.getCamLayOut(), SIGNAL(stoped(QString)), this, SLOT(onLayOutStoped(QString)));


	connect(&m_videoView, SIGNAL(onPressed(QString, QString)), this, SLOT(onItemPressed(QString, QString)));

	m_videoView.getCamLayOut().setContent(startscreen_content());
	m_videoView.getCamLayOut().setDecoration(int(GraphicsView::NONE));
	m_videoView.getCamLayOut().setName(start_screen);
	
	m_videoView.getCamLayOut().start();
}



MainWnd::~MainWnd()
{
	CLDeviceManager::instance().getDiveceSercher().wait();
	m_videoView.getCamLayOut().stop();
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

void MainWnd::onItemPressed(QString layoutname, QString itemname)
{

	m_lastCommand = itemname;
	m_lastLayoutName = layoutname;

	if (m_lastLayoutName == start_screen)
	{
		if (m_lastCommand==button_logo)
			m_videoView.getCamLayOut().stop(true);
	}
	else if (m_lastLayoutName == video_layout)
	{
		if (m_lastCommand==button_home)
			m_videoView.getCamLayOut().stop(true);
	}


}

void MainWnd::onLayOutStoped(QString layoutname)
{

	if (layoutname == start_screen)
	{
		if (m_lastCommand==button_logo)
		{
			
			m_videoView.getCamLayOut().setContent(LayoutContent());
			m_videoView.getCamLayOut().setDecoration(int(GraphicsView::VIDEO));
			m_videoView.getCamLayOut().setName(video_layout);
			m_videoView.getCamLayOut().start();
		
		}
	}
	else if (layoutname == video_layout)
	{
		if (m_lastCommand==button_home)
		{
			m_videoView.getCamLayOut().setContent(startscreen_content());
			m_videoView.getCamLayOut().setDecoration(int(GraphicsView::NONE));
			m_videoView.getCamLayOut().setName(start_screen);
			m_videoView.getCamLayOut().start();
		}
	}


	m_lastCommand = "";

}
//====================================================================
//====================================================================

