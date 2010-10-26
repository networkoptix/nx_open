#include "mainwnd.h"
#include <QVBoxLayout>
#include "ui/video_cam_layout/start_screen_content.h"
#include "settings.h"
#include "ui/layout_navigator.h"
#include "ui/video_cam_layout/layout_manager.h"

extern QString button_layout;
extern QString button_home;

MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags):
//QMainWindow(parent, flags),
mMode(UNDEFINED_ViewMode),
m_normalView(0),
m_topView(0),
m_bottomView(0),
m_editedView(0)
{
	//ui.setupUi(this);
	setWindowTitle("HDWitness");

	setAutoFillBackground(false);

	QPalette palette;
	palette.setColor(backgroundRole(), app_bkr_color);
	setPalette(palette);

	//setWindowOpacity(.80);

	//=======================================================

	//goToLayouteditor();
	goToNomalLayout();

	
	//=======================================================

	


	const int min_wisth = 400;
	setMinimumWidth(min_wisth);
	setMinimumHeight(min_wisth*3/4);

	
	toggleFullScreen();

}



MainWnd::~MainWnd()
{
	destroyNavigator(m_normalView);
	destroyNavigator(m_topView);
	destroyNavigator(m_bottomView);
	destroyNavigator(m_editedView);

	CLDeviceManager::instance().getDiveceSercher().wait();
}

void MainWnd::goToNomalLayout()
{
	if (mMode==NORMAL_ViewMode) // already there
		return;

	
	

	//=======remove====
	if (true)
	{
		destroyNavigator(m_topView);
		destroyNavigator(m_bottomView);
		destroyNavigator(m_editedView);

		QLayout* mainl = layout();
		delete mainl;
	}


	//=======add====
	m_normalView = new CLLayoutNavigator(this);
	QLayout* l = new QHBoxLayout();
	l->addWidget(&(m_normalView->getView()));

	m_normalView->setMode(NORMAL_ViewMode);
	mMode=NORMAL_ViewMode;

	connect(m_normalView, SIGNAL(onItemPressed(QString)), this, SLOT(onItemPressed(QString)));

	setLayout(l);

}

void MainWnd::goToLayouteditor()
{
	if (mMode==LAYOUTEDITOR_ViewMode) // already there
		return;

	//=======remove====
	if (m_normalView)
	{
		QLayout* mainl = layout();

		//mMainLayout->removeWidget(&(m_normalView->getView()));

		delete mainl;
		destroyNavigator(m_normalView);
	}


	//=======add====
	m_topView = new CLLayoutNavigator(this,  CLSceneLayoutManager::instance().getAllLayoutsContent()); 
	m_topView->getView().setMaximumWidth(600);
	m_topView->getView().setMaximumHeight(500);

	m_bottomView = new CLLayoutNavigator(this, CLSceneLayoutManager::instance().getEmptyLayoutContent()); 
	m_bottomView->getView().setMaximumWidth(600);

	m_editedView = new CLLayoutNavigator(this, CLSceneLayoutManager::instance().getEmptyLayoutContent());

	connect(m_topView, SIGNAL(onItemPressed(QString)), this, SLOT(onItemPressed(QString)));
	connect(m_bottomView, SIGNAL(onItemPressed(QString)), this, SLOT(onItemPressed(QString)));
	connect(m_editedView, SIGNAL(onItemPressed(QString)), this, SLOT(onItemPressed(QString)));


	connect(m_topView, SIGNAL(onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*)), this, SLOT(onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*)));
	connect(m_bottomView, SIGNAL(onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*)), this, SLOT(onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*)));

	m_topView->setMode(LAYOUTEDITOR_ViewMode);
	m_bottomView->setMode(LAYOUTEDITOR_ViewMode);
	m_editedView->setMode(NORMAL_ViewMode);


	QLayout* ml = new QHBoxLayout();

	QLayout* VLayout = new QVBoxLayout;
	VLayout->addWidget(&(m_topView->getView()));
	VLayout->addWidget(&(m_bottomView->getView()));

	ml->addItem(VLayout);
	ml->addWidget(&(m_editedView->getView()));
	setLayout(ml);

	mMode = LAYOUTEDITOR_ViewMode;

}


void MainWnd::closeEvent ( QCloseEvent * event )
{
	destroyNavigator(m_normalView);
	destroyNavigator(m_topView);
	destroyNavigator(m_bottomView);
	destroyNavigator(m_editedView);

}


void MainWnd::toggleFullScreen()
{
	if (!isFullScreen())
		showFullScreen();
	else
		showMaximized();
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

void MainWnd::onItemPressed(QString itemname)
{
	if (itemname==button_layout && mMode==NORMAL_ViewMode)
	{
		QTimer::singleShot(20, this, SLOT(goToLayouteditor()));
	}

	if (itemname==button_home && mMode==LAYOUTEDITOR_ViewMode)
	{
		QTimer::singleShot(20, this, SLOT(goToNomalLayout()));
	}

}

void MainWnd::onNewLayoutItemSelected(CLLayoutNavigator* ln, LayoutContent* newl)
{
	if (mMode!=LAYOUTEDITOR_ViewMode)
		return;

	if (ln==m_topView)
	{
		m_bottomView->goToNewLayoutContent(newl);
	}
}