#include "mainwnd.h"
#include "ui/video_cam_layout/start_screen_content.h"
#include "settings.h"
#include "ui/layout_navigator.h"
#include "ui/video_cam_layout/layout_manager.h"

extern QString button_layout;
extern QString button_home;

MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags):
//QMainWindow(parent, flags),
m_normalView(0)
{
	//ui.setupUi(this);
	setWindowTitle("HDWitness");

	setAutoFillBackground(false);

	QPalette palette;
	palette.setColor(backgroundRole(), app_bkr_color);
	setPalette(palette);

	//setWindowOpacity(.80);

	//=======================================================

	const int min_wisth = 800;
	setMinimumWidth(min_wisth);
	setMinimumHeight(min_wisth*3/4);

	//=======add====
	m_normalView = new CLLayoutNavigator(this);
	QLayout* l = new QHBoxLayout();
	l->addWidget(&(m_normalView->getView()));

	m_normalView->setMode(NORMAL_ViewMode);
	m_normalView->getView().setViewMode(GraphicsView::NormalView);

	setLayout(l);
	showFullScreen();
}

MainWnd::~MainWnd()
{
	destroyNavigator(m_normalView);
	CLDeviceManager::instance().getDiveceSercher().wait();
}

void MainWnd::closeEvent ( QCloseEvent * event )
{
	destroyNavigator(m_normalView);
}

void MainWnd::destroyNavigator(CLLayoutNavigator*& nav)
{
	if (nav) 
	{
		nav->destroy();
		delete nav;
		nav = 0;
	}

}
