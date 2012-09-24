#include "main_window.h"

#include <QtCore/QFile>
#include <QtGui/QApplication>
#include <QtGui/QBoxLayout>
#include <QtGui/QFileDialog>
#include <QtGui/QToolButton>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>
#include <QtGui/QFileOpenEvent>
#include <QtNetwork/QNetworkReply>

#include <utils/common/warnings.h>
#include <utils/common/event_processors.h>
#include <utils/common/environment.h>

#include <core/resource_managment/resource_discovery_manager.h>
#include <core/resource_managment/resource_pool.h>

#include <api/app_server_connection.h>
#include <api/session_manager.h>

#include "ui/common/frame_section.h"
#include "ui/actions/action_manager.h"
#include "ui/graphics/view/graphics_view.h"
#include "ui/graphics/view/gradient_background_painter.h"
#include "ui/workbench/handlers/workbench_action_handler.h"
#include "ui/workbench/handlers/workbench_panic_handler.h"
#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_grid_mapper.h"
#include "ui/workbench/workbench_layout.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_ui.h"
#include "ui/workbench/workbench_synchronizer.h"
#include "ui/workbench/workbench_context.h"
#include "ui/workbench/workbench_resource.h"
#include "ui/processors/drag_processor.h"
#include "ui/style/skin.h"
#include "ui/style/globals.h"
#include "ui/style/proxy_style.h"
#include "ui/events/system_menu_event.h"

#include "file_processor.h"
#include "utils/settings.h"

#include "resource_tree_widget.h"
#include "dwm.h"
#include "layout_tab_bar.h"

namespace {

    QToolButton *newActionButton(QAction *action, qreal sizeMultiplier = 1.0) {
        QToolButton *button = new QToolButton();
        button->setDefaultAction(action);

        qreal aspectRatio = QnGeometry::aspectRatio(action->icon().actualSize(QSize(1024, 1024)));
        int iconHeight = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, 0, button) * sizeMultiplier;
        int iconWidth = iconHeight * aspectRatio;
        button->setFixedSize(iconWidth, iconHeight);

        return button;
    }

    void setVisibleRecursively(QLayout *layout, bool visible) {
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

} // anonymous namespace


QnMainWindow::QnMainWindow(QnWorkbenchContext *context, QWidget *parent, Qt::WindowFlags flags): 
    base_type(parent, flags | Qt::CustomizeWindowHint),
    QnWorkbenchContextAware(context),
    m_controller(0),
    m_titleVisible(true),
    m_dwm(NULL),
    m_drawCustomFrame(false)
{
    setAttribute(Qt::WA_AlwaysShowToolTips);

    /* We want to receive system menu event on Windows. */
    QnSystemMenuEvent::initialize();

    /* And file open events on Mac. */
    QnSingleEventSignalizer *fileOpenSignalizer = new QnSingleEventSignalizer(this);
    fileOpenSignalizer->setEventType(QEvent::FileOpen);
    qApp->installEventFilter(fileOpenSignalizer);
    connect(fileOpenSignalizer,             SIGNAL(activated(QObject *, QEvent *)),         this,                                   SLOT(at_fileOpenSignalizer_activated(QObject *, QEvent *)));

    /* Set up dwm. */
    m_dwm = new QnDwm(this);

    connect(m_dwm,                          SIGNAL(compositionChanged(bool)),               this,                                   SLOT(updateDwmState()));

    /* Set up properties. */
    setWindowTitle(QApplication::applicationName());
    setAcceptDrops(true);
    setMinimumWidth(minimalWindowWidth);
    setMinimumHeight(minimalWindowHeight);
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, Qt::black);
        setPalette(palette);
    }


    /* Set up scene & view. */
    QGraphicsScene *scene = new QGraphicsScene(this);
    m_view = new QnGraphicsView(scene);
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

    m_backgroundPainter.reset(new QnGradientBackgroundPainter(120.0, this));
    m_view->installLayerPainter(m_backgroundPainter.data(), QGraphicsScene::BackgroundLayer);


    /* Set up model & control machinery. */
    display()->setScene(scene);
    display()->setView(m_view);
    display()->setNormalMarginFlags(Qn::MarginsAffectSize | Qn::MarginsAffectPosition);

    m_controller = new QnWorkbenchController(this);
    m_ui = new QnWorkbenchUi(this);
    m_ui->setFlags(QnWorkbenchUi::HideWhenZoomed | QnWorkbenchUi::AdjustMargins);


    /* Set up handlers. */
    QnWorkbenchActionHandler *actionHandler = context->instance<QnWorkbenchActionHandler>();
    actionHandler->setWidget(this);


    /* Set up actions. */
    addAction(action(Qn::NextLayoutAction));
    addAction(action(Qn::PreviousLayoutAction));
    addAction(action(Qn::SaveCurrentLayoutAction));
    addAction(action(Qn::SaveCurrentLayoutAsAction));
    addAction(action(Qn::ExitAction));
    addAction(action(Qn::FullscreenAction));
    addAction(action(Qn::AboutAction));
    addAction(action(Qn::SystemSettingsAction));
    addAction(action(Qn::OpenFileAction));
    addAction(action(Qn::ConnectToServerAction));
    addAction(action(Qn::OpenNewTabAction));
    addAction(action(Qn::OpenNewWindowAction));
    addAction(action(Qn::CloseLayoutAction));
    addAction(action(Qn::MainMenuAction));
    addAction(action(Qn::YouTubeUploadAction));
    addAction(action(Qn::EditTagsAction));
    addAction(action(Qn::OpenInFolderAction));
    addAction(action(Qn::RemoveLayoutItemAction));
    addAction(action(Qn::RemoveFromServerAction));
    addAction(action(Qn::SelectAllAction));
    addAction(action(Qn::TakeScreenshotAction));
    addAction(action(Qn::IncrementDebugCounterAction));
    addAction(action(Qn::DecrementDebugCounterAction));
    addAction(action(Qn::TogglePanicModeAction));

    connect(action(Qn::MaximizeAction),     SIGNAL(toggled(bool)),                          this,                                   SLOT(setMaximized(bool)));
    connect(action(Qn::FullscreenAction),   SIGNAL(toggled(bool)),                          this,                                   SLOT(setFullScreen(bool)));
    connect(action(Qn::MinimizeAction),     SIGNAL(triggered()),                            this,                                   SLOT(minimize()));

    menu()->setTargetProvider(m_ui);


    /* Tab bar. */
    m_tabBar = new QnLayoutTabBar(this);
    m_tabBar->setAttribute(Qt::WA_TranslucentBackground);

    connect(m_tabBar,                       SIGNAL(closeRequested(QnWorkbenchLayout *)),    this,                                   SLOT(at_tabBar_closeRequested(QnWorkbenchLayout *)));


    /* Tab bar layout. To snap tab bar to graphics view. */
    QVBoxLayout *tabBarLayout = new QVBoxLayout();
    tabBarLayout->setContentsMargins(0, 0, 0, 0);
    tabBarLayout->setSpacing(0);
    tabBarLayout->addStretch(0x1000);
    tabBarLayout->addWidget(m_tabBar);

    /* We want the main menu button to activate the corresponding action so that
     * main menu is updated. However, menu buttons do not activate their corresponding 
     * actions as they do not receive release events. We work this around by making
     * some hacky connections. */
    m_mainMenuButton = newActionButton(action(Qn::MainMenuAction), 1.5);
    m_mainMenuButton->setPopupMode(QToolButton::InstantPopup);

    disconnect(m_mainMenuButton,            SIGNAL(pressed()),                              m_mainMenuButton,                       SLOT(_q_buttonPressed()));
    connect(m_mainMenuButton,               SIGNAL(pressed()),                              m_mainMenuButton->defaultAction(),      SLOT(trigger()));
    connect(m_mainMenuButton,               SIGNAL(pressed()),                              m_mainMenuButton,                       SLOT(_q_buttonPressed()));

    /* Layout for window buttons that can be removed from the title bar. */
    m_windowButtonsLayout = new QHBoxLayout();
    m_windowButtonsLayout->setContentsMargins(0, 0, 0, 0);
    m_windowButtonsLayout->setSpacing(2);
    m_windowButtonsLayout->addSpacing(6);
    m_windowButtonsLayout->addWidget(newActionButton(action(Qn::MinimizeAction)));
    m_windowButtonsLayout->addWidget(newActionButton(action(Qn::FullscreenAction)));
    m_windowButtonsLayout->addWidget(newActionButton(action(Qn::ExitAction)));

    /* Title layout. We cannot create a widget for title bar since there appears to be
     * no way to make it transparent for non-client area windows messages. */
    m_titleLayout = new QHBoxLayout();
    m_titleLayout->setContentsMargins(0, 0, 0, 0);
    m_titleLayout->setSpacing(2);
    m_titleLayout->addWidget(m_mainMenuButton);
    m_titleLayout->addLayout(tabBarLayout);
    m_titleLayout->addWidget(newActionButton(action(Qn::OpenNewTabAction)));
    m_titleLayout->addStretch(0x1000);
    m_titleLayout->addWidget(newActionButton(action(Qn::TogglePanicModeAction)));
    m_titleLayout->addSpacing(6);
    m_titleLayout->addWidget(newActionButton(action(Qn::ConnectToServerAction)));
    m_titleLayout->addLayout(m_windowButtonsLayout);

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

    /* Post-initialize. */
    updateDwmState();
    setOptions(TitleBarDraggable | WindowButtonsVisible);

    /* Open single tab. */
    action(Qn::OpenNewTabAction)->trigger();
}

QnMainWindow::~QnMainWindow() {
    m_dwm = NULL;
}

void QnMainWindow::setTitleVisible(bool visible) {
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

void QnMainWindow::setWindowButtonsVisible(bool visible) {
    bool currentVisibility = m_windowButtonsLayout->parent() != NULL;
    if(currentVisibility == visible)
        return;

    if(visible) {
        m_titleLayout->addLayout(m_windowButtonsLayout);
        setVisibleRecursively(m_windowButtonsLayout, true);
    } else {
        m_titleLayout->takeAt(m_titleLayout->count() - 1);
        m_windowButtonsLayout->setParent(NULL);
        setVisibleRecursively(m_windowButtonsLayout, false);
    }
}

void QnMainWindow::setMaximized(bool maximized) {
    if(maximized == isMaximized())
        return;

    if(maximized) {
        showMaximized();
    } else if(isMaximized()) {
        showNormal();
    }
}

void QnMainWindow::setFullScreen(bool fullScreen) {
    if(fullScreen == isFullScreen())
        return;

    if(fullScreen) {
        showFullScreen();
    } else if(isFullScreen()) {
        showNormal();
    }
}

void QnMainWindow::toggleFullScreen() {
    setFullScreen(!isFullScreen());
}

void QnMainWindow::minimize() {
    setWindowState(Qt::WindowMinimized | windowState());
}

void QnMainWindow::toggleTitleVisibility() {
    setTitleVisible(!isTitleVisible());
}

void QnMainWindow::handleMessage(const QString &message) {
    const QStringList files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    
    menu()->trigger(Qn::DropResourcesAction, QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(files)));
}

QnMainWindow::Options QnMainWindow::options() const {
    return m_options;
}

void QnMainWindow::setOptions(Options options) {
    if(m_options == options)
        return;

    m_options = options;

    setWindowButtonsVisible(m_options & WindowButtonsVisible);
    m_ui->setWindowButtonsUsed(m_options & WindowButtonsVisible);
}

void QnMainWindow::updateDecorationsState() {
    bool fullScreen = isFullScreen();
    bool maximized = isMaximized();

    action(Qn::FullscreenAction)->setChecked(fullScreen);
    action(Qn::MaximizeAction)->setChecked(maximized);

    setTitleVisible(!fullScreen);
    m_ui->setTitleUsed(fullScreen);
    m_view->setLineWidth(isFullScreen() ? 0 : 1);

    updateDwmState();
}

void QnMainWindow::updateDwmState() {
    if(isFullScreen()) {
        /* Full screen mode. */
        m_drawCustomFrame = false;
        m_frameMargins = QMargins(0, 0, 0, 0);

        if(m_dwm->isSupported()) {
            setAttribute(Qt::WA_NoSystemBackground, false);
            setAttribute(Qt::WA_TranslucentBackground, false);

            m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
            m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
            m_dwm->disableBlurBehindWindow();
        }

        /* Can't set to (0, 0, 0, 0) on Windows as in fullScreen mode context menu becomes invisible.
         * Looks like Qt bug: http://bugreports.qt.nokia.com/browse/QTBUG-7556. */
#ifdef Q_OS_WIN
        setContentsMargins(0, 0, 0, 1);
#else
        setContentsMargins(0, 0, 0, 0);
#endif

        m_titleLayout->setContentsMargins(0, 0, 0, 0);
        m_viewLayout->setContentsMargins(0, 0, 0, 0);
    } else if(m_dwm->isSupported() && m_dwm->isCompositionEnabled()) {
        /* Windowed with aero glass. */
        m_drawCustomFrame = false;
        m_frameMargins = m_dwm->themeFrameMargins();

        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);

        m_dwm->extendFrameIntoClientArea();
        m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->enableBlurBehindWindow();

        setContentsMargins(0, 0, 0, 0);

        m_titleLayout->setContentsMargins(m_frameMargins.left(), 2, m_frameMargins.right(), 0);
        m_viewLayout->setContentsMargins(
            m_frameMargins.left(),
            isTitleVisible() ? 0 : m_frameMargins.top(),
            m_frameMargins.right(),
            m_frameMargins.bottom()
        );
    } else {
        /* Windowed without aero glass. */
        m_drawCustomFrame = true;
        m_frameMargins = m_dwm->isSupported() ? m_dwm->themeFrameMargins() : QMargins(8, 8, 8, 8);

        if(m_dwm->isSupported()) {
            setAttribute(Qt::WA_NoSystemBackground, false);
            setAttribute(Qt::WA_TranslucentBackground, false);

            m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
            m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
            m_dwm->disableBlurBehindWindow();
        }

        m_titleLayout->setContentsMargins(m_frameMargins.left(), 2, m_frameMargins.right(), 0);
        m_viewLayout->setContentsMargins(
            m_frameMargins.left(),
            isTitleVisible() ? 0 : m_frameMargins.top(),
            m_frameMargins.right(),
            m_frameMargins.bottom()
        );
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnMainWindow::event(QEvent *event) {
    bool result = base_type::event(event);

    if(event->type() == QnSystemMenuEvent::SystemMenu) {
        if(m_mainMenuButton->isVisible())
            m_mainMenuButton->click();
            
        QApplication::sendEvent(m_ui, event);
        result = true;
    }

    if(m_dwm != NULL)
        result |= m_dwm->widgetEvent(event);

    return result;
}

void QnMainWindow::mouseReleaseEvent(QMouseEvent *event) {
    base_type::mouseReleaseEvent(event);

    if(event->button() == Qt::RightButton && windowFrameSectionAt(event->pos()) == Qt::TitleBarArea) {
        QContextMenuEvent e(QContextMenuEvent::Mouse, m_tabBar->mapFrom(this, event->pos()), event->globalPos(), event->modifiers());
        QApplication::sendEvent(m_tabBar, &e);
        event->accept();
    }
}

void QnMainWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    base_type::mouseDoubleClickEvent(event);

    if(event->button() == Qt::LeftButton && windowFrameSectionAt(event->pos()) == Qt::TitleBarArea) {
        toggleFullScreen();
        event->accept();
    }
}

void QnMainWindow::changeEvent(QEvent *event) {
    if(event->type() == QEvent::WindowStateChange)
        updateDecorationsState();

    base_type::changeEvent(event);
}

void QnMainWindow::paintEvent(QPaintEvent *event) {
    base_type::paintEvent(event);

    if(m_drawCustomFrame) {
        QPainter painter(this);

        painter.setPen(QPen(qnGlobals->frameColor(), 3));
        painter.drawRect(QRect(
            0,
            0,
            width() - 1,
            height() - 1
        ));
    }
}

void QnMainWindow::dragEnterEvent(QDragEnterEvent *event) {
    m_dropResources = QnWorkbenchResource::deserializeResources(event->mimeData());
    if (m_dropResources.empty())
        return;

    event->acceptProposedAction();
}

void QnMainWindow::dragMoveEvent(QDragMoveEvent *event) {
    if(m_dropResources.empty())
        return;

    event->acceptProposedAction();
}

void QnMainWindow::dragLeaveEvent(QDragLeaveEvent *) {
    m_dropResources = QnResourceList();
}

void QnMainWindow::dropEvent(QDropEvent *event) {
    menu()->trigger(Qn::DropResourcesIntoNewLayoutAction, m_dropResources);

    event->acceptProposedAction();
}

#ifdef Q_OS_WIN
bool QnMainWindow::winEvent(MSG *message, long *result)
{
    if(m_dwm->widgetWinEvent(message, result))
        return true;

    return base_type::winEvent(message, result);
}
#endif

Qt::WindowFrameSection QnMainWindow::windowFrameSectionAt(const QPoint &pos) const {
    if(isFullScreen())
        return Qt::NoSection;

    Qt::WindowFrameSection result = Qn::toNaturalQtFrameSection(Qn::calculateRectangularFrameSections(rect(), QnGeometry::eroded(rect(), m_frameMargins), QRect(pos, pos)));
    if((m_options & TitleBarDraggable) && result == Qt::NoSection && pos.y() <= m_tabBar->mapTo(const_cast<QnMainWindow *>(this), m_tabBar->rect().bottomRight()).y())
        result = Qt::TitleBarArea;
    return result;
}

void QnMainWindow::at_fileOpenSignalizer_activated(QObject *, QEvent *event) {
    if(event->type() != QEvent::FileOpen) {
        qnWarning("Expected event of type %1, received an event of type %2.", static_cast<int>(QEvent::FileOpen), static_cast<int>(event->type()));
        return;
    }

    handleMessage(static_cast<QFileOpenEvent *>(event)->file());
}

void QnMainWindow::at_tabBar_closeRequested(QnWorkbenchLayout *layout) {
    QnWorkbenchLayoutList layouts;
    layouts.push_back(layout);
    menu()->trigger(Qn::CloseLayoutAction, layouts);
}

