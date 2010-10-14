#include "mainwnd.h"
#include <QVBoxLayout>
#include "ui/video_cam_layout/start_screen_content.h"
#include "settings.h"



MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags):
//QMainWindow(parent, flags),
m_layout(this)
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
	mainLayout->addWidget(&m_layout.getView());
	setLayout(mainLayout);

	//=======================================================


	const int min_wisth = 400;
	setMinimumWidth(min_wisth);
	setMinimumHeight(min_wisth*3/4);

	
	toggleFullScreen();

}



MainWnd::~MainWnd()
{
	CLDeviceManager::instance().getDiveceSercher().wait();
	
}

void MainWnd::closeEvent ( QCloseEvent * event )
{
	m_layout.destroy();
	
}


void MainWnd::toggleFullScreen()
{
	if (!isFullScreen())
		showFullScreen();
	else
		showMaximized();
}

