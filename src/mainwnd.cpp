#include "mainwnd.h"
#include "ui/video_cam_layout/start_screen_content.h"
#include "settings.h"
#include "ui/layout_navigator.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "device/directory_browser.h"
#include "device_plugins/archive/filetypesupport.h"
#include "util.h"

#include "device_plugins/archive/avi_files/avi_dvd_device.h"
#include "device_plugins/archive/avi_files/avi_bluray_device.h"
#include "device_plugins/archive/avi_files/avi_dvd_strem_reader.h"

extern QString button_layout;
extern QString button_home;

void findAcceptedFiles(QStringList& files, const QString& path)
{
    if (CLAviDvdDevice::isAcceptedUrl(path))
    {
        if (path.indexOf(QLatin1Char('?')) == -1)
        {
            // open all titles on DVD
            QStringList titles = CLAVIDvdStreamReader::getTitleList(path);
            foreach(QString title, titles)
                files << path + QLatin1String("?title=") + title;
        }
        else {
            files.append(path);
        }
    }
    else if (CLAviBluRayDevice::isAcceptedUrl(path))
    {
        files.append(path);
    }
    else
    {
        FileTypeSupport fileTypeSupport;
        QFileInfo fileInfo(path);
        if (fileInfo.isDir())
        {
            QDirIterator iter(path, QDirIterator::Subdirectories);
            while (iter.hasNext())
            {
                QString nextFilename = iter.next();
                if (QFileInfo(nextFilename).isFile())
                {
                    if (fileTypeSupport.isFileSupported(nextFilename))
                    {
                        files.append(nextFilename);
                    }
                }
            }
        }
        else if (fileInfo.isFile())
        {
            if (fileTypeSupport.isFileSupported(path))
            {
                files.append(path);
            }
        }
    }
}

MainWnd::MainWnd(int argc, char* argv[], QWidget *parent, Qt::WFlags flags):
//QMainWindow(parent, flags),
m_normalView(0)
{
    Q_UNUSED(parent);
    Q_UNUSED(flags);

	setAcceptDrops(true);
	//ui.setupUi(this);
	setWindowTitle(tr("HDWitness"));

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
        QString fileName = fromNativePath(QString::fromLocal8Bit(argv[i]));
        findAcceptedFiles(files, fileName);
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
#ifdef Q_OS_WIN
    l->setContentsMargins(0, 1, 0, 0);
#else
    l->setContentsMargins(0, 0, 0, 0);
#endif

	m_normalView->setMode(NORMAL_ViewMode);
	m_normalView->getView().setViewMode(GraphicsView::NormalView);

    setLayout(l);
    if (!files.isEmpty())
        show();
    else
        showFullScreen();
}

MainWnd::~MainWnd()
{
	destroyNavigator(m_normalView);
	CLDeviceManager::instance().getDeviceSearcher().wait();
}

void MainWnd::addFilesToCurrentOrNewLayout(const QStringList& files, bool forceNewLayout)
{
    if (files.isEmpty())
        return;

	cl_log.log(QLatin1String("Entering addFilesToCurrentOrNewLayout"), cl_logALWAYS);

	CLDeviceManager::instance().addFiles(files);

    // If current content created by opening files or DND, use it. Otherwise create new one.
    LayoutContent* content = m_normalView->getView().getCamLayOut().getContent();

    if (!forceNewLayout && content != CLSceneLayoutManager::instance().getSearchLayout() &&
        content != CLSceneLayoutManager::instance().startScreenLayoutContent())
    {
        cl_log.log(QLatin1String("Using old layout, content ") + content->getName(), cl_logALWAYS);
        foreach(QString file, files)
        {
            m_normalView->getView().getCamLayOut().addDevice(file, true);
            content->addDevice(file);
        }

        m_normalView->getView().fitInView(600, 100, SLOW_START_SLOW_END);
    } else
    {
        cl_log.log(QLatin1String("Creating new layout, content ") + content->getName(), cl_logALWAYS);
        content = CLSceneLayoutManager::instance().getNewEmptyLayoutContent();

        foreach(QString file, files)
        {
            content->addDevice(file);
        }

        m_normalView->goToNewLayoutContent(content);
    }
}

void MainWnd::handleMessage(const QString& message)
{
    QStringList files = message.trimmed().split(QLatin1Char('\0'), QString::SkipEmptyParts);

    addFilesToCurrentOrNewLayout(files);
    raise();
    activateWindow();
}

void MainWnd::closeEvent ( QCloseEvent * /*event*/ )
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

void MainWnd::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWnd::dropEvent(QDropEvent *event)
{
    QStringList files;
    foreach (QUrl url, event->mimeData()->urls())
    {
        QString filename = url.toLocalFile();

        findAcceptedFiles(files, filename);
    }

    addFilesToCurrentOrNewLayout(files, event->keyboardModifiers() & Qt::AltModifier);
    raise();
    activateWindow();
}
