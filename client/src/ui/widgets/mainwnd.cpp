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

#include "ui/widgets/navigationtreewidget.h"

#include "ui/style/skin.h"
#include "ui/style/globals.h"
#include "ui/style/proxystyle.h"

#include <utils/common/warnings.h>

#include "file_processor.h"
#include "settings.h"

#include "dwm.h"
#include "layout_tab_bar.h"

#include "eventmanager.h"

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
      m_titleVisible(true),
      m_dwm(NULL)
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

    connect(&cm_new_tab, SIGNAL(triggered()), this, SLOT(at_newLayoutRequested()));
    addAction(&cm_new_tab);

    connect(SessionManager::instance(), SIGNAL(error(int)), this, SLOT(at_sessionManager_error(int)));

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
    const QSizeF defaultCellSize = QSizeF(15000.0, 10000.0); /* Graphics scene has problems with handling mouse events on small scales, so the larger these numbers, the better. */
    const QSizeF defaultSpacing = QSizeF(2500.0, 2500.0);
    m_workbench = new QnWorkbench(this);
    m_workbench->mapper()->setCellSize(defaultCellSize);
    m_workbench->mapper()->setSpacing(defaultSpacing);

    m_display = new QnWorkbenchDisplay(m_workbench, this);
    m_display->setScene(scene);
    m_display->setView(m_view);
    m_display->setMarginFlags(Qn::MARGINS_AFFECT_POSITION);

    m_controller = new QnWorkbenchController(m_display, this);
    m_ui = new QnWorkbenchUi(m_display, this);
    m_ui->setFlags(QnWorkbenchUi::HIDE_WHEN_ZOOMED | QnWorkbenchUi::AFFECT_MARGINS_WHEN_NORMAL);

    m_ui->treeWidget()->setWorkbenchController(m_controller); // TODO: smells bad.
    connect(m_ui->treeWidget(), SIGNAL(newTabRequested()), this, SLOT(at_newLayoutRequested()));
    connect(m_ui->treeWidget(), SIGNAL(activated(uint)), this, SLOT(at_treeWidget_activated(uint)));
    connect(m_ui,               SIGNAL(titleBarDoubleClicked()), this, SLOT(toggleFullScreen()));

    /* Tab bar. */
    m_tabBar = new QnLayoutTabBar(this);
    m_tabBar->setAttribute(Qt::WA_TranslucentBackground);
    connect(m_tabBar, SIGNAL(currentChanged(QnWorkbenchLayout *)), this, SLOT(at_tabBar_currentChanged(QnWorkbenchLayout *)));
    connect(m_tabBar, SIGNAL(layoutRemoved(QnWorkbenchLayout *)), this, SLOT(at_tabBar_layoutRemoved(QnWorkbenchLayout *)));

    /* Tab bar layout. To snap tab bar to graphics view. */
    m_tabBarLayout = new QVBoxLayout();
    m_tabBarLayout->setContentsMargins(0, 0, 0, 0);
    m_tabBarLayout->setSpacing(0);
    m_tabBarLayout->addStretch(0x1000);
    m_tabBarLayout->addWidget(m_tabBar);

    /* Title layout. We cannot create a widget for title bar since there appears to be
     * no way to make it transparent for non-client area windows messages. */
    m_titleLayout = new QHBoxLayout();
    m_titleLayout->setContentsMargins(0, 0, 0, 0);
    m_titleLayout->setSpacing(0);
    m_titleLayout->addLayout(m_tabBarLayout);
    m_titleLayout->addWidget(newActionButton(&cm_new_tab));
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
    at_newLayoutRequested();

    /* Process input files. */
    for (int i = 1; i < argc; ++i)
        m_controller->drop(QFile::decodeName(argv[i]));

    showFullScreen();
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

void MainWnd::handleMessage(const QString &message)
{
    const QStringList files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    
    m_controller->drop(files);
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
            QnEventManager::instance()->stop();
            SessionManager::instance()->stop();

            QnAppServerConnectionFactory::setDefaultUrl(connection.url);

            // repopulate the resource pool
            QnResource::stopCommandProc();
            QnResourceDiscoveryManager::instance().stop();

            // don't remove local resources
            const QnResourceList remoteResources = qnResPool->getResourcesWithFlag(QnResource::remote);
            qnResPool->removeResources(remoteResources);

            SessionManager::instance()->start();
            QnEventManager::instance()->run();

            QnResourceDiscoveryManager::instance().start();
            QnResource::startCommandProc();
        }
    }
    delete dialog;
    dialog = 0;
}

void MainWnd::updateFullScreenState() 
{
    bool fullScreen = isFullScreen();

    setTitleVisible(!fullScreen);
    m_ui->setTitleUsed(fullScreen);
    if(fullScreen) {
        m_globalLayout->setContentsMargins(0, 0, 0, 0);
        m_tabBar->setParent(NULL);
        m_ui->setTabBar(m_tabBar);
        m_tabBar->show();
    } else {
        m_globalLayout->setContentsMargins(0, 0, 0, 0);
        m_ui->setTabBar(NULL);
        m_tabBarLayout->addWidget(m_tabBar);
        m_tabBar->show();
    }
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

        m_dwm->disableSystemWindowPainting();
        m_dwm->disableTransparentErasing();
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

        m_dwm->disableSystemWindowPainting();
        m_dwm->enableTransparentErasing();
        m_dwm->extendFrameIntoClientArea();
        m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->enableBlurBehindWindow(); /* For reasons unknown, this call is also needed to prevent display artifacts. */
        m_dwm->enableDoubleClickProcessing();
        m_dwm->enableTitleBarDrag();

        m_dwm->enableFrameEmulation();
        m_dwm->setEmulatedFrameMargins(frameMargins);
        m_dwm->setEmulatedTitleBarHeight(0x1000); /* So that window is click-draggable no matter where the user clicked. */

        setContentsMargins(0, 0, 0, 0);

        m_titleLayout->setContentsMargins(frameMargins.left(), 2, 2, 0);
        m_viewLayout->setContentsMargins(
            frameMargins.left(),
            isTitleVisible() ? 0 : frameMargins.top(),
            frameMargins.right(),
            frameMargins.bottom()
        );
    } else {
        /* Windowed without aero glass. */

        m_drawCustomFrame = true;

        QMargins frameMargins = m_dwm->themeFrameMargins();

        setAttribute(Qt::WA_NoSystemBackground, false);
        setAttribute(Qt::WA_TranslucentBackground, false);

        m_dwm->disableSystemWindowPainting();
        m_dwm->disableTransparentErasing();
        m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
        m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->disableBlurBehindWindow();
        m_dwm->enableDoubleClickProcessing();
        m_dwm->enableTitleBarDrag();

        m_dwm->enableFrameEmulation();
        m_dwm->setEmulatedFrameMargins(frameMargins);
        m_dwm->setEmulatedTitleBarHeight(0x1000);

        setContentsMargins(0, 0, 0, 0);

        m_titleLayout->setContentsMargins(frameMargins.left(), 2, 2, 0);
        m_viewLayout->setContentsMargins(
            frameMargins.left(),
            isTitleVisible() ? 0 : frameMargins.top(),
            frameMargins.right(),
            frameMargins.bottom()
        );
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void MainWnd::at_sessionManager_error(int error)
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

void MainWnd::at_newLayoutRequested()
{
    QnWorkbenchLayout *layout = new QnWorkbenchLayout(this);

    m_tabBar->addLayout(layout);
    m_tabBar->setCurrentLayout(layout);
}

void MainWnd::at_tabBar_currentChanged(QnWorkbenchLayout *layout)
{
    m_workbench->setLayout(layout);
    m_display->fitInView(false);

    /* This one is important. If we don't unset the transformation anchor, viewport position will be messed up when show event is delivered. */
    m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
}

void MainWnd::at_tabBar_layoutRemoved(QnWorkbenchLayout *layout) 
{
    delete layout;
}

void MainWnd::at_treeWidget_activated(uint resourceId)
{
    QnResourcePtr resource = qnResPool->getResourceById(QnId(QString::number(resourceId))); // TODO: bad, makes assumptions on QnId internals.
    m_controller->drop(resource);
}

bool MainWnd::event(QEvent *event) {
    bool result = base_type::event(event);

    if(m_dwm != NULL)
        result |= m_dwm->widgetEvent(event);

    return result;
}

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

void MainWnd::resizeEvent(QResizeEvent *event) {
    QRect rect = this->rect();
    QRect viewGeometry = m_view->geometry();

    m_dwm->setNonErasableContentMargins(QMargins(
        viewGeometry.left(),
        viewGeometry.top(),
        rect.right() - viewGeometry.right(),
        rect.bottom() - viewGeometry.bottom()
    ));

    base_type::resizeEvent(event);
}

#ifdef Q_OS_WIN
bool MainWnd::winEvent(MSG *message, long *result)
{
    if(m_dwm->widgetWinEvent(message, result))
        return true;

    return base_type::winEvent(message, result);
}
#endif

