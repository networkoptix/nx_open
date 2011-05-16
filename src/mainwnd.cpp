#include "mainwnd.h"
#include "ui/video_cam_layout/start_screen_content.h"
#include "settings.h"
#include "ui/layout_navigator.h"
#include "ui/video_cam_layout/layout_manager.h"

extern QString button_layout;
extern QString button_home;

MainWnd::MainWnd(int argc, char* argv[], QWidget *parent, Qt::WFlags flags):
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

    QStringList files;
    for (int i = 1; i < argc; ++i)
    {
        if (QFile(argv[i]).exists())
            files.append(argv[i]);
    }

    LayoutContent* content = 0;
    
    if (!files.isEmpty())
    {
        CLDeviceManager::instance().addFiles(files);

        content = CLSceneLayoutManager::instance().getNewEmptyLayoutContent();

        foreach(QString file, files)
        {
            content->addDevice(file);
        }
    }

	//=======add====
	m_normalView = new CLLayoutNavigator(this, content);
	QLayout* l = new QHBoxLayout();
	l->addWidget(&(m_normalView->getView()));

    // Can't set 0,0,0,0 on Windows as in fullScreen mode context menu becomes invisible
    // Looks like QT bug: http://bugreports.qt.nokia.com/browse/QTBUG-7556
#ifdef _WIN32	
    l->setContentsMargins(0, 1, 0, 0);
#else
	l->setContentsMargins(0, 0, 0, 0);
#endif
	
	m_normalView->setMode(NORMAL_ViewMode);
	m_normalView->getView().setViewMode(GraphicsView::NormalView);

	setLayout(l);
	showFullScreen();
}

MainWnd::~MainWnd()
{
	destroyNavigator(m_normalView);
	CLDeviceManager::instance().getDeviceSearcher().wait();
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
