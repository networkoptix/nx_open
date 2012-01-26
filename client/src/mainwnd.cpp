#include "mainwnd.h"

#include <QtCore/QFile>

#include <QtGui/QApplication>
#include <QtGui/QBoxLayout>
#include <QtGui/QFileDialog>
#include <QtGui/QToolBar>
#include <QtGui/QToolButton>
#include <QtGui/QCloseEvent>

#include <QtNetwork/QNetworkReply>

#include <api/AppServerConnection.h>
#include <api/SessionManager.h>

#include <core/resourcemanagment/resource_discovery_manager.h>
#include <core/resourcemanagment/resource_pool.h>

#include "ui/context_menu_helper.h"
#include "ui/navigationtreewidget.h"

#include "ui/dialogs/aboutdialog.h"
#include "ui/dialogs/logindialog.h"
#include "ui/preferences/preferencesdialog.h"

#include "ui/mixins/sync_play_mixin.h"
#include "ui/mixins/render_watch_mixin.h"

#include "ui/graphics/view/graphics_view.h"
#include "ui/graphics/view/blue_background_painter.h"

#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_grid_mapper.h"
#include "ui/workbench/workbench_layout.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_ui.h"

#include "ui/widgets3/tabwidget.h"

#include "ui/skin/skin.h"
#include "ui/skin/globals.h"
#include "ui/proxystyle.h"

#include <utils/common/warnings.h>

#include "file_processor.h"
#include "settings.h"

#include "ui/dwm.h"

Q_DECLARE_METATYPE(QnWorkbenchLayout *);

namespace {

    QToolButton *newActionButton(QAction *action)
    {
        QToolButton *button = new QToolButton();
        button->setDefaultAction(action);

        int iconSize = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, 0, button);
        button->setIconSize(QSize(iconSize, iconSize));

        /* Tool buttons may end up being non-square o_O. We don't allow that. */
        QSize sizeHint = button->sizeHint();
        int buttonSize = qMin(sizeHint.width(), sizeHint.height());
        button->setFixedSize(buttonSize, buttonSize);

        return button;
    }

    void setVisibleRecursively(QLayout *layout, bool visible)
    {
        for(int i = 0, count = layout->count(); i < count; i++) {
            QLayoutItem *item = layout->itemAt(i);
            if(item->widget()) {
                item->widget()->setVisible(visible);
            } else if(item->layout()) {
                setVisibleRecursively(item->layout(), visible);
            }
        }
    }

    int minimalWindowWidth = 800;
    int minimalWindowHeight = 600;

}

MainWnd::MainWnd(int argc, char* argv[], QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags | Qt::CustomizeWindowHint),
      m_controller(0),
      m_drawCustomFrame(false),
      m_titleVisible(true)
{
    /* Set up dwm. */
    m_dwm = new QnDwm(this);

    connect(m_dwm, SIGNAL(compositionChanged(bool)), this, SLOT(updateDwmState()));
    connect(m_dwm, SIGNAL(titleBarDoubleClicked()), this, SLOT(toggleFullScreen()));

    /* Set up QWidget. */
    setWindowTitle(QApplication::applicationName());
    setAcceptDrops(true);
    setMinimumWidth(minimalWindowWidth);
    setMinimumHeight(minimalWindowHeight);
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, Qt::black);
        setPalette(palette);
    }

    /* Set up actions. */
    connect(&cm_exit, SIGNAL(triggered()), this, SLOT(close()));
    addAction(&cm_exit);

    connect(&cm_toggle_fullscreen, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));
    addAction(&cm_toggle_fullscreen);

    connect(&cm_about, SIGNAL(triggered()), this, SLOT(showAboutDialog()));
    addAction(&cm_about);

    connect(&cm_preferences, SIGNAL(triggered()), this, SLOT(showPreferencesDialog()));
    addAction(&cm_preferences);

    connect(&cm_open_file, SIGNAL(triggered()), this, SLOT(showOpenFileDialog()));
    addAction(&cm_open_file);

    connect(&cm_reconnect, SIGNAL(triggered()), this, SLOT(showAuthenticationDialog()));
    addAction(&cm_reconnect);

    connect(SessionManager::instance(), SIGNAL(error(int)), this, SLOT(handleSessionManagerError(int)));

    /* Set up scene & view. */
    QGraphicsScene *scene = new QGraphicsScene(this);
    m_view = new QnGraphicsView(scene);
    m_view->setPaintFlags(QnGraphicsView::BACKGROUND_DONT_INVOKE_BASE | QnGraphicsView::FOREGROUND_DONT_INVOKE_BASE);
    m_view->setFrameStyle(QFrame::Box | QFrame::Plain);
    m_view->setLineWidth(1);
    m_view->setAutoFillBackground(true);
    {
        /* Adjust palette so that inherited background painting is not needed. */
        QPalette palette = m_view->palette();
        palette.setColor(QPalette::Background, Qt::black);
        palette.setColor(QPalette::Base, Qt::black);
        m_view->setPalette(palette);
    }

    m_backgroundPainter.reset(new QnBlueBackgroundPainter(120.0));
    m_view->installLayerPainter(m_backgroundPainter.data(), QGraphicsScene::BackgroundLayer);

    /* Set up model & control machinery. */
    const QSizeF defaultCellSize = QSizeF(150.0, 100.0);
    const QSizeF defaultSpacing = QSizeF(25.0, 25.0);
    m_workbench = new QnWorkbench(this);
    m_workbench->mapper()->setCellSize(defaultCellSize);
    m_workbench->mapper()->setSpacing(defaultSpacing);

    m_display = new QnWorkbenchDisplay(m_workbench, this);
    m_display->setScene(scene);
    m_display->setView(m_view);
    m_display->setMarginFlags(Qn::MARGINS_AFFECT_POSITION);

    m_controller = new QnWorkbenchController(m_display, this);
    m_ui = new QnWorkbenchUi(m_display, this);

    m_ui->treeWidget()->setWorkbenchController(m_controller); // TODO: smells bad.
    connect(m_ui->treeWidget(), SIGNAL(newTabRequested()), this, SLOT(newLayout()));
    connect(m_ui->treeWidget(), SIGNAL(activated(uint)), this, SLOT(at_treeWidget_activated(uint)));

    /* Tab bar. */
    m_tabBar = new QTabBar();
    m_tabBar->setMovable(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    m_tabBar->setDrawBase(false);
    m_tabBar->setShape(QTabBar::TriangularNorth);

    connect(m_tabBar, SIGNAL(currentChanged(int)), this, SLOT(setCurrentLayout(int)));
    connect(m_tabBar, SIGNAL(tabCloseRequested(int)), this, SLOT(removeLayout(int)));

    /* New tab button. */
    QToolButton *newTabButton = new QToolButton();
    newTabButton->setToolTip(tr("New Tab"));
    newTabButton->setShortcut(QKeySequence::New);
    newTabButton->setIcon(Skin::icon(QLatin1String("plus.png")));
    newTabButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connect(newTabButton, SIGNAL(clicked()), this, SLOT(newLayout()));

    /* Tab bar layout. To snap tab bar to graphics view. */
    QVBoxLayout *tabBarLayout = new QVBoxLayout();
    tabBarLayout->setContentsMargins(0, 0, 0, 0);
    tabBarLayout->setSpacing(0);
    tabBarLayout->addStretch(0x1000);
    tabBarLayout->addWidget(m_tabBar);

    /* Title layout. We cannot create a widget for title bar since there appears to be
     * no way to make it transparent for non-client area windows messages. */
    m_titleLayout = new QHBoxLayout();
    m_titleLayout->setContentsMargins(0, 0, 0, 0);
    m_titleLayout->setSpacing(0);
    m_titleLayout->addLayout(tabBarLayout);
    m_titleLayout->addWidget(newTabButton);
    m_titleLayout->addStretch(0x1000);
    m_titleLayout->addWidget(newActionButton(&cm_reconnect));
    m_titleLayout->addWidget(newActionButton(&cm_preferences));
    m_titleLayout->addWidget(newActionButton(&cm_toggle_fullscreen));
    m_titleLayout->addWidget(newActionButton(&cm_exit));

    /* Layouts. */
    m_viewLayout = new QVBoxLayout();
    m_viewLayout->setContentsMargins(0, 0, 0, 0);
    m_viewLayout->setSpacing(0);
    m_viewLayout->addWidget(m_view);

    m_globalLayout = new QVBoxLayout();
    m_globalLayout->setContentsMargins(0, 0, 0, 0);
    m_globalLayout->setSpacing(0);
    m_globalLayout->addLayout(m_titleLayout);
    m_globalLayout->addLayout(m_viewLayout);
    m_globalLayout->setStretchFactor(m_viewLayout, 0x1000);

    setLayout(m_globalLayout);

    /* Add single tab. */
    newLayout();

    /* Process input files. */
    for (int i = 1; i < argc; ++i)
        m_controller->drop(QFile::decodeName(argv[i]));

    //showFullScreen();
    updateDwmState();
}

MainWnd::~MainWnd()
{
    return;
}

void MainWnd::setTitleVisible(bool visible) 
{
    if(m_titleVisible == visible)
        return;

    m_titleVisible = visible;
    if(visible) {
        m_globalLayout->insertLayout(0, m_titleLayout);
        setVisibleRecursively(m_titleLayout, true);
    } else {
        m_globalLayout->takeAt(0);
        m_titleLayout->setParent(NULL);
        setVisibleRecursively(m_titleLayout, false);
    }

    updateDwmState();
}

void MainWnd::setFullScreen(bool fullScreen) 
{
    if(fullScreen == isFullScreen())
        return;

    if(fullScreen) {
        showFullScreen();
    } else {
        showNormal();
    }
}

void MainWnd::toggleFullScreen() 
{
    setFullScreen(!isFullScreen());
}

void MainWnd::toggleTitleVisibility() 
{
    setTitleVisible(!isTitleVisible());
}

void MainWnd::newLayout()
{
    int index = m_tabBar->addTab(Skin::icon(QLatin1String("decorations/square-view.png")), tr("Scene"));

    m_tabBar->setCurrentIndex(index);
}

QnWorkbenchLayout *MainWnd::layoutForIndex(int index) {
    QnWorkbenchLayout *result = m_tabBar->tabData(index).value<QnWorkbenchLayout *>();
    if(result == NULL) {
        result = new QnWorkbenchLayout(this);
        m_tabBar->setTabData(index, QVariant::fromValue<QnWorkbenchLayout *>(result));
    }

    return result;
}

void MainWnd::setCurrentLayout(int index)
{
    if(index < 0 || index >= m_tabBar->count()) {
        qnWarning("Layout index out of bounds: %1 not in [%2, %3).", index, 0, m_tabBar->count());
        return;
    }

    if(m_tabBar->currentIndex() != index)
        m_tabBar->setCurrentIndex(index);

    m_workbench->setLayout(layoutForIndex(index));
    m_display->fitInView(false);

    /* This one is important. If we don't unset the transformation anchor, viewport position will be messed up when show event is delivered. */
    m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
}

void MainWnd::removeLayout(int index)
{
    if(index < 0 || index >= m_tabBar->count()) {
        qnWarning("Layout index out of bounds: %1 not in [%2, %3).", index, 0, m_tabBar->count());
        return;
    }

    if (m_tabBar->count() <= 1)
        return; /* Don't remove the last layout. */

    if(m_tabBar->currentIndex() == index)
        m_workbench->setLayout(NULL);

    QnWorkbenchLayout *layout = m_tabBar->tabData(index).value<QnWorkbenchLayout *>();
    delete layout;

    m_tabBar->removeTab(index);
}

void MainWnd::handleMessage(const QString &message)
{
    const QStringList files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    
    m_controller->drop(files);
}

void MainWnd::at_treeWidget_activated(uint resourceId)
{
    QnResourcePtr resource = qnResPool->getResourceById(QnId(QString::number(resourceId))); // TODO: bad, makes assumptions on QnId internals.
    m_controller->drop(resource);
}

void MainWnd::showOpenFileDialog()
{
    QFileDialog dialog(this, tr("Open file"));
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    QStringList filters;
    filters << tr("All Supported (*.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp *.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("Video (*.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp)");
    filters << tr("Pictures (*.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("All files (*.*)");
    dialog.setNameFilters(filters);
    if (dialog.exec())
        m_controller->drop(dialog.selectedFiles());
}

void MainWnd::showAboutDialog()
{
    AboutDialog dialog(this);
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.exec();
}

void MainWnd::showPreferencesDialog()
{
    PreferencesDialog dialog(this);
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.exec();
}

void MainWnd::handleSessionManagerError(int error)
{
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError: // ### remove ?
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::AuthenticationRequiredError:
    case QNetworkReply::UnknownNetworkError:
        // Do not show popup box! It's annoying!
        // Show something like color label in the main window.
        // showAuthenticationDialog();
        break;

    default:
        break;
    }
}

void MainWnd::showAuthenticationDialog()
{
    static LoginDialog *dialog = 0;
    if (dialog)
        return;

    const QUrl lastUsedUrl = Settings::lastUsedConnection().url;
    if (lastUsedUrl.isValid() && lastUsedUrl != QnAppServerConnectionFactory::defaultUrl())
        return;

    dialog = new LoginDialog(this);
    dialog->setModal(true);
    if (dialog->exec()) {
        const Settings::ConnectionData connection = Settings::lastUsedConnection();
        if (connection.url.isValid()) {
            QnAppServerConnectionFactory::setDefaultUrl(connection.url);

            // repopulate the resource pool
            QnResource::stopCommandProc();
            QnResourceDiscoveryManager::instance().stop();

            // don't remove local resources
            const QnResourceList remoteResources = qnResPool->getResourcesWithFlag(QnResource::remote);
            qnResPool->removeResources(remoteResources);

            QnResourceDiscoveryManager::instance().start();
            QnResource::startCommandProc();
        }
    }
    delete dialog;
    dialog = 0;
}

void MainWnd::updateFullScreenState() {
    bool fullScreen = isFullScreen();

    setTitleVisible(!fullScreen);
    m_ui->setTitleUsed(fullScreen);
    m_view->setLineWidth(isFullScreen() ? 0 : 1);

    updateDwmState();
}

void MainWnd::updateDwmState()
{
    if(!m_dwm->isSupported()) {
        m_drawCustomFrame = false;

        setAttribute(Qt::WA_NoSystemBackground, false);
        setAttribute(Qt::WA_TranslucentBackground, false);

        m_titleLayout->setContentsMargins(0, 0, 0, 0);
        m_viewLayout->setContentsMargins(0, 0, 0, 0);

        return; /* Do nothing on systems where our tricks are not supported. */
    }

    if(isFullScreen()) {
        /* Full screen mode. */

        m_drawCustomFrame = false;

        setAttribute(Qt::WA_NoSystemBackground, false);
        setAttribute(Qt::WA_TranslucentBackground, false);

        m_dwm->disableSystemFramePainting();
        m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
        m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->disableBlurBehindWindow();
        m_dwm->enableDoubleClickProcessing();
        m_dwm->disableTitleBarDrag();

        m_dwm->enableFrameEmulation();
        m_dwm->setEmulatedFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->setEmulatedTitleBarHeight(0x1000); /* So that window is click-draggable no matter where the user clicked. */

        /* Can't set to (0, 0, 0, 0) on Windows as in fullScreen mode context menu becomes invisible.
         * Looks like Qt bug: http://bugreports.qt.nokia.com/browse/QTBUG-7556. */
#ifdef Q_OS_WIN
        setContentsMargins(0, 0, 0, 1);
#else
        setContentsMargins(0, 0, 0, 0);
#endif

        m_titleLayout->setContentsMargins(0, 0, 0, 0);
        m_viewLayout->setContentsMargins(0, 0, 0, 0);
    } else if(m_dwm->isCompositionEnabled()) {
        /* Windowed with aero glass. */

        m_drawCustomFrame = false;

        if(!m_dwm->isCompositionEnabled())
            qnWarning("Transitioning to glass state when aero composition is disabled. Expect display artifacts.");

        QMargins frameMargins = m_dwm->themeFrameMargins();

        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);

        m_dwm->disableSystemFramePainting();
        m_dwm->extendFrameIntoClientArea();
        m_dwm->setCurrentFrameMargins(QMargins(1, 0, 1, 1)); /* Can't set (0, 0, 0, 0) here as it will cause awful display artifacts. */
        m_dwm->enableBlurBehindWindow(); /* For reasons unknown, this call is also needed to prevent display artifacts. */
        m_dwm->enableDoubleClickProcessing();
        m_dwm->enableTitleBarDrag();

        m_dwm->enableFrameEmulation();
        m_dwm->setEmulatedFrameMargins(frameMargins);
        m_dwm->setEmulatedTitleBarHeight(0x1000); /* So that window is click-draggable no matter where the user clicked. */

        setContentsMargins(0, 0, 0, 0);

        m_titleLayout->setContentsMargins(frameMargins.left() - 1, 2, 2, 0);
        m_viewLayout->setContentsMargins(
            frameMargins.left() - 1,
            isTitleVisible() ? 0 : frameMargins.top(),
            frameMargins.right() - 1,
            frameMargins.bottom() - 1
        );
    } else {
        /* Windowed without aero glass. */

        m_drawCustomFrame = true;

        QMargins frameMargins = m_dwm->themeFrameMargins();

        setAttribute(Qt::WA_NoSystemBackground, false);
        setAttribute(Qt::WA_TranslucentBackground, false);

        m_dwm->disableSystemFramePainting();
        m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
        m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->disableBlurBehindWindow();
        m_dwm->enableDoubleClickProcessing();
        m_dwm->enableTitleBarDrag();

        m_dwm->enableFrameEmulation();
        m_dwm->setEmulatedFrameMargins(frameMargins);
        m_dwm->setEmulatedTitleBarHeight(0x1000);

        setContentsMargins(0, 0, 0, 0);

        m_titleLayout->setContentsMargins(frameMargins.left() - 1, 2, 2, 0);
        m_viewLayout->setContentsMargins(
            frameMargins.left(),
            isTitleVisible() ? 0 : frameMargins.top(),
            frameMargins.right(),
            frameMargins.bottom() - 1
        );
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void MainWnd::closeEvent(QCloseEvent *event)
{
    base_type::closeEvent(event);

    if (event->isAccepted())
        Q_EMIT mainWindowClosed();
}

void MainWnd::changeEvent(QEvent *event) 
{
    if(event->type() == QEvent::WindowStateChange)
        updateFullScreenState();

    base_type::changeEvent(event);
}

void MainWnd::paintEvent(QPaintEvent *event) 
{
    base_type::paintEvent(event);

    /* Draw frame if needed. */
    if(m_drawCustomFrame) {
        QPainter painter(this);

        painter.setPen(QPen(Globals::frameColor(), 3));
        painter.drawRect(QRect(
            0,
            0,
            width() - 1,
            height() - 1
        ));
    }
}

#ifdef Q_OS_WIN
bool MainWnd::winEvent(MSG *message, long *result)
{
    if(m_dwm->winEvent(message, result))
        return true;

    return base_type::winEvent(message, result);
}
#endif
