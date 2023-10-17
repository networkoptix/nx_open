// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "main_window_title_bar_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QDragMoveEvent>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QMenu>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <finders/systems_finder.h>
#include <nx/utils/app_info.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/screen_recording_indicator.h>
#include <nx/vms/client/desktop/common/widgets/tool_button.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_tab_bar/system_tab_bar.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <ui/common/palette.h>
#include <ui/widgets/cloud_status_panel.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {

static const int kResizerHeight = 4;
static const QSize kControlButtonSize(
    QnMainWindowTitleBarWidget::kSystemBarHeight, QnMainWindowTitleBarWidget::kSystemBarHeight);
static const QSize kTabBarButtonSize(
    QnMainWindowTitleBarWidget::kLayoutBarHeight, QnMainWindowTitleBarWidget::kLayoutBarHeight);
static const QMargins kSingleLevelVLineContentMargins = {0, 0, 0, 0};
static const QMargins kDoubleLevelVLineContentMargins = {0, 5, 0, 5};

QFrame* newVLine(const QString& lightColorName, const QString& shadowColorName)
{
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Raised);
    line->setFixedWidth(2);
    setPaletteColor(line, QPalette::Light, core::colorTheme()->color(lightColorName));
    setPaletteColor(line, QPalette::Shadow, core::colorTheme()->color(shadowColorName));
    return line;
}

QFrame* newVLine(int verticalMargin = 0, const QString& colorName = "")
{
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedWidth(1);
    line->setContentsMargins(0, verticalMargin, 0, verticalMargin);
    if (!colorName.isEmpty())
        setPaletteColor(line, QPalette::Shadow, core::colorTheme()->color(colorName));
    return line;
}

void executeButtonMenu(ToolButton* invoker, QMenu* menu, const QPoint& offset = QPoint(0, 0))
{
    if (!menu || !invoker)
        return;

    menu->setProperty(nx::style::Properties::kMenuNoMouseReplayArea,
        QVariant::fromValue(QPointer<QWidget>(invoker)));

    QnHiDpiWorkarounds::showMenu(menu,
        QnHiDpiWorkarounds::safeMapToGlobal(invoker,
            invoker->rect().bottomLeft() + offset));

    invoker->setDown(false);
}

/* If a widget is visible directly or within a graphics proxy: */
bool isWidgetVisible(const QWidget* widget)
{
    if (!widget->isVisible())
        return false; //< widget is not visible to the screen or a proxy

    if (const auto proxy = widget->window()->graphicsProxyWidget())
    {
        if (!proxy->isVisible() || !proxy->scene())
            return false; //< proxy is not visible or doesn't belong to a scene

        for (const auto view: proxy->scene()->views())
        {
            if (isWidgetVisible(view))
                return true; //< scene is visible
        }

        return false; // scene has no visible views
    }

    return true; //< widget is visible directly to the screen
}

} // namespace

class QnMainWindowTitleBarWidgetPrivate: public QObject
{
    QnMainWindowTitleBarWidget* q_ptr;
    Q_DECLARE_PUBLIC(QnMainWindowTitleBarWidget)

public:
    QnMainWindowTitleBarWidgetPrivate(QnMainWindowTitleBarWidget* parent);

    void setSkipDoubleClick();

public:
    ToolButton* mainMenuButton = nullptr;
    QnLayoutTabBar* layoutBar = nullptr;
    SystemTabBar* systemBar = nullptr;
    ToolButton* newTabButton = nullptr;
    ToolButton* currentLayoutsButton = nullptr;
    QnCloudStatusPanel* cloudPanel = nullptr;
    std::unique_ptr<MimeData> mimeData;
    bool skipDoubleClickFlag = false;
    QnMainWindowTitleBarResizerWidget* resizer = nullptr;
    QHBoxLayout* systemLayout = nullptr;
    QHBoxLayout* tabLayout = nullptr;
    QWidget* tabPlaceholder = nullptr;
    QList<QWidget*> widgetsToTransfer;
    QSharedPointer<QnMainWindowTitleBarWidget::Store> store;

    /** There are some rare scenarios with looping menus. */
    bool isMenuOpened = false;

    QSharedPointer<QMenu> mainMenuHolder;

    // Inner state of the widget: expanded and homeTabActive:
    bool expanded = true;
    bool homeTabActive = true;
};

QnMainWindowTitleBarWidgetPrivate::QnMainWindowTitleBarWidgetPrivate(
    QnMainWindowTitleBarWidget* parent)
    :
    QObject(parent),
    q_ptr(parent)
{
}

void QnMainWindowTitleBarWidgetPrivate::setSkipDoubleClick()
{
    skipDoubleClickFlag = true;
}

QnMainWindowTitleBarWidget::QnMainWindowTitleBarWidget(
    WindowContext* windowContext,
    QWidget* parent)
    :
    base_type(parent),
    WindowContextAware(windowContext),
    d_ptr(new QnMainWindowTitleBarWidgetPrivate(this))
{
    Q_D(QnMainWindowTitleBarWidget);

    setFocusPolicy(Qt::NoFocus);
    setAutoFillBackground(true);
    setAcceptDrops(true);
    const QColor windowColor = core::colorTheme()->color("dark7");
    setPaletteColor(this, QPalette::Base, windowColor);
    // Workaround for behavior change Qt5 -> Qt6.
    // Force QPalette detach to prevent inheritance of parent brush, see QTBUG-98762.
    setPaletteColor(this, QPalette::Window, QColor::fromRgb(~windowColor.rgba()));
    setPaletteColor(this, QPalette::Window, windowColor);

    d->mainMenuButton = newActionButton(
        menu::MainMenuAction,
        qnSkin->icon("titlebar/main_menu.svg", "", nullptr, Style::kTitleBarSubstitutions),
        HelpTopic::Id::MainWindow_TitleBar_MainMenu);
    connect(d->mainMenuButton, &ToolButton::justPressed, this,
        [this]()
        {
            menu()->trigger(menu::MainMenuAction);
        });

    connect(action(menu::MainMenuAction), &QAction::triggered, this,
        [this]()
        {
            Q_D(QnMainWindowTitleBarWidget);
            if (!isWidgetVisible(d->mainMenuButton))
                return;

            if (d->isMenuOpened)
                return;
            QScopedValueRollback<bool> guard(d->isMenuOpened, true);

            static const QPoint kVerticalOffset(0, 2);
            d->mainMenuHolder.reset(menu()->newMenu(menu::MainScope, mainWindowWidget()));
            d->mainMenuButton->setDown(true);
            executeButtonMenu(d->mainMenuButton, d->mainMenuHolder.data(), kVerticalOffset);
        },
        Qt::QueuedConnection); //< QueuedConnection is needed here because 2 title bars
                               // (windowed/welcomescreen and fullscreen) are subscribed to
                               // MainMenuAction, and main menu must not change
                               // windowed/welcomescreen/fullscreen state BETWEEN their slots
                               // processing MainMenuAction.
                               //
                               // TODO: #vkutin #gdm #ynikitenkov Lift this limitation in the
                               // future

    d->layoutBar = new QnLayoutTabBar(windowContext, this);
    d->layoutBar->setFocusPolicy(Qt::NoFocus);
    d->layoutBar->setFixedHeight(kLayoutBarHeight);
    connect(d->layoutBar, &QnLayoutTabBar::tabCloseRequested, d,
        &QnMainWindowTitleBarWidgetPrivate::setSkipDoubleClick);

    d->cloudPanel = new QnCloudStatusPanel(windowContext, this);
    d->cloudPanel->setFocusPolicy(Qt::NoFocus);

    const QString styleSheet = QString("QToolButton:hover { background-color: %1 }")
        .arg(core::colorTheme()->color("dark8").name());
    d->newTabButton = newActionButton(
        menu::OpenNewTabAction,
        HelpTopic::Id::MainWindow_TitleBar_NewLayout,
        kTabBarButtonSize);
    d->newTabButton->setStyleSheet(styleSheet);

    d->currentLayoutsButton = newActionButton(
        menu::OpenCurrentUserLayoutMenu,
        kTabBarButtonSize);
    d->currentLayoutsButton->setStyleSheet(styleSheet);
    connect(d->currentLayoutsButton, &ToolButton::justPressed, this,
        [this]()
        {
            Q_D(QnMainWindowTitleBarWidget);

            if (d->isMenuOpened)
                return;
            QScopedValueRollback<bool> guard(d->isMenuOpened, true);

            QScopedPointer<QMenu> layoutsMenu(menu()->newMenu(
                menu::OpenCurrentUserLayoutMenu,
                menu::TitleBarScope,
                mainWindowWidget()));

            executeButtonMenu(d->currentLayoutsButton, layoutsMenu.data());
        });

    if (ini().enableMultiSystemTabBar)
        initMultiSystemTabBar();
    else
        initLayoutsOnlyTabBar();
}

QnMainWindowTitleBarWidget::~QnMainWindowTitleBarWidget()
{
}

int QnMainWindowTitleBarWidget::y() const
{
    return pos().y();
}

void QnMainWindowTitleBarWidget::setY(int y)
{
    QPoint pos_ = pos();

    if (pos_.y() != y)
    {
        move(pos_.x(), y);
        emit yChanged(y);
    }
}

QnLayoutTabBar* QnMainWindowTitleBarWidget::tabBar() const
{
    Q_D(const QnMainWindowTitleBarWidget);
    return d->layoutBar;
}

void QnMainWindowTitleBarWidget::setStateStore(QSharedPointer<Store> store)
{
    Q_D(QnMainWindowTitleBarWidget);
    d->store = store;
    connect(this, &QnMainWindowTitleBarWidget::toggled,
        store.data(), &MainWindowTitleBarStateStore::setExpanded);
    connect(this, &QnMainWindowTitleBarWidget::homeTabActivationChanged,
        store.data(), &MainWindowTitleBarStateStore::setHomeTabActive);
    connect(store.data(), &MainWindowTitleBarStateStore::stateChanged, this,
        [this](MainWindowTitleBarState state)
        {
            if (state.expanded)
                expand();
            else
                collapse();

            setHomeTabActive(state.homeTabActive);
        });
}

QSharedPointer<QnMainWindowTitleBarWidget::Store> QnMainWindowTitleBarWidget::stateStore() const
{
    Q_D(const QnMainWindowTitleBarWidget);
    return d->store;
}

void QnMainWindowTitleBarWidget::setTabBarStuffVisible(bool visible)
{
    Q_D(const QnMainWindowTitleBarWidget);
    d->layoutBar->setVisible(visible);
    d->newTabButton->setVisible(visible);
    d->currentLayoutsButton->setVisible(visible);
    action(menu::OpenNewTabAction)->setEnabled(visible);
}

void QnMainWindowTitleBarWidget::setHomeTabActive(bool isActive)
{
    Q_D(QnMainWindowTitleBarWidget);
    if (d->homeTabActive == isActive)
        return;

    if (isActive)
    {
        if (d->expanded)
        {
            for (auto widget: d->widgetsToTransfer)
                widget->hide();
            d->systemBar->show();
        }
        setFixedHeight(kSystemBarHeight);
        d->systemBar->activateHomeTab();
    }
    else
    {
        d->systemBar->activatePreviousTab();
        for (auto widget: d->widgetsToTransfer)
            widget->show();
        if (!d->expanded)
            d->systemBar->hide();
        else
            setFixedHeight(kFullTitleBarHeight);
    }
    d->resizer->setVisible(!isActive);
    d->homeTabActive = isActive;
    emit homeTabActivationChanged(isActive);
}

bool QnMainWindowTitleBarWidget::isExpanded() const
{
    Q_D(const QnMainWindowTitleBarWidget);
    return d->expanded;
}

bool QnMainWindowTitleBarWidget::isSystemTabBarUpdating() const
{
    Q_D(const QnMainWindowTitleBarWidget);
    return d->systemBar && d->systemBar->isUpdating();
}

void QnMainWindowTitleBarWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    base_type::mouseDoubleClickEvent(event);

#ifndef Q_OS_MACX
    Q_D(QnMainWindowTitleBarWidget);
        if (d->skipDoubleClickFlag)
    {
        d->skipDoubleClickFlag = false;
        return;
    }

    menu()->trigger(menu::EffectiveMaximizeAction);
    event->accept();
#endif
}

void QnMainWindowTitleBarWidget::dragEnterEvent(QDragEnterEvent* event)
{
    Q_D(QnMainWindowTitleBarWidget);
    d->mimeData.reset(new MimeData{event->mimeData()});
    if (d->mimeData->isEmpty())
        return;

    event->acceptProposedAction();
}

void QnMainWindowTitleBarWidget::dragMoveEvent(QDragMoveEvent* event)
{
    Q_D(QnMainWindowTitleBarWidget);
    if (d->mimeData->isEmpty())
        return;

    if (!qBetween(d->layoutBar->pos().x(), event->pos().x(), d->cloudPanel->pos().x()))
        return;

    event->acceptProposedAction();
}

void QnMainWindowTitleBarWidget::dragLeaveEvent(QDragLeaveEvent* /*event*/)
{
    Q_D(QnMainWindowTitleBarWidget);
    d->mimeData.reset();
}

void QnMainWindowTitleBarWidget::dropEvent(QDropEvent* event)
{
    Q_D(QnMainWindowTitleBarWidget);
    if (d->mimeData->isEmpty())
        return;

    const auto systemContext = system();
    if (!systemContext)
        return;

    const auto showreels = systemContext->showreelManager()->showreels(d->mimeData->entities());
    for (const auto& showreel: showreels)
        menu()->trigger(menu::ReviewShowreelAction, {Qn::UuidRole, showreel.id});

    systemContext->resourcePool()->addNewResources(d->mimeData->resources());

    menu()->triggerIfPossible(menu::OpenInNewTabAction, d->mimeData->resources());

    d->mimeData.reset();
    event->acceptProposedAction();
}

ToolButton *QnMainWindowTitleBarWidget::newActionButton(
    menu::IDType actionId,
    const QIcon& icon,
    int helpTopicId)
{
    auto button = new ToolButton(this);

    button->setDefaultAction(action(actionId));
    button->setFocusPolicy(Qt::NoFocus);
    button->setIcon(icon);
    button->adjustIconSize();
    button->setFixedSize(qnSkin->maximumSize(icon));
    button->setAutoRaise(true);

    if (helpTopicId != HelpTopic::Id::Empty)
        setHelpTopic(button, helpTopicId);

    return button;
}

ToolButton* QnMainWindowTitleBarWidget::newActionButton(
    menu::IDType actionId,
    int helpTopicId,
    const QSize& fixedSize)
{
    auto button = new ToolButton(this);

    button->setDefaultAction(action(actionId));
    button->setFocusPolicy(Qt::NoFocus);
    button->adjustIconSize();
    button->setFixedSize(fixedSize.isEmpty() ? button->iconSize() : fixedSize);
    button->setAutoRaise(true);

    if (helpTopicId != HelpTopic::Id::Empty)
        setHelpTopic(button, helpTopicId);

    return button;
}

ToolButton* QnMainWindowTitleBarWidget::newActionButton(
    menu::IDType actionId,
    const QSize& fixedSize)
{
    return newActionButton(actionId, HelpTopic::Id::Empty, fixedSize);
}

QWidget* QnMainWindowTitleBarWidget::newRecordingIndicator(const QSize& fixedSize)
{
    auto screenRecordingAction = action(menu::ToggleScreenRecordingAction);
    if (!screenRecordingAction)
        return nullptr;

    auto indicator = new ScreenRecordingIndicator(this);

    connect(
        screenRecordingAction, &QAction::triggered,
        indicator, &QWidget::setVisible);

    connect(indicator, &QAbstractButton::clicked, this,
        [this]
        {
            menu()->trigger(menu::ToggleScreenRecordingAction);
        });

    indicator->setFixedSize(fixedSize);
    indicator->setVisible(screenRecordingAction->isChecked());

    return indicator;
}

void QnMainWindowTitleBarWidget::initMultiSystemTabBar()
{
    Q_D(QnMainWindowTitleBarWidget);

    d->systemBar = new SystemTabBar(this);
    d->systemBar->setFocusPolicy(Qt::NoFocus);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    d->systemLayout = new QHBoxLayout();
    d->tabLayout = new QHBoxLayout();
    d->tabLayout->setContentsMargins(0, 0, 0, 0);
    d->tabLayout->setSpacing(0);

    d->tabPlaceholder = new QWidget(this);
    d->tabPlaceholder->setStyleSheet(QString(".QWidget{ background-color: %1 }")
        .arg(core::colorTheme()->color("dark10").name()));

    mainLayout->addLayout(d->systemLayout);
    mainLayout->addWidget(d->tabPlaceholder);
    d->tabPlaceholder->setLayout(d->tabLayout);

    d->systemLayout->addWidget(d->mainMenuButton);
    d->systemLayout->addWidget(newVLine(0, "dark8"));
    d->systemLayout->addWidget(d->systemBar);
    d->systemLayout->addStretch(1);
    d->systemLayout->addSpacing(80);
    d->systemLayout->addWidget(newVLine("dark8", "dark6"));
    d->systemLayout->addWidget(d->cloudPanel);
    d->systemLayout->addWidget(newVLine("dark8", "dark6"));
    if (auto indicator = newRecordingIndicator(kControlButtonSize))
    {
        d->systemLayout->addWidget(indicator);
        d->systemLayout->addWidget(newVLine("dark8", "dark6"));
    }
    d->systemLayout->addWidget(newActionButton(
        menu::WhatsThisAction,
        HelpTopic::Id::MainWindow_ContextHelp,
        kControlButtonSize));
    d->systemLayout->addWidget(newVLine("dark8", "dark6"));
    d->systemLayout->addWidget(newActionButton(
        menu::MinimizeAction,
        kControlButtonSize));
    d->systemLayout->addWidget(newVLine("dark8", "dark6"));
    d->systemLayout->addWidget(newActionButton(
        menu::EffectiveMaximizeAction,
        HelpTopic::Id::MainWindow_Fullscreen,
        kControlButtonSize));
    d->systemLayout->addWidget(newVLine("dark8", "dark6"));
    {
        const QColor background = "#212A2F";
        const core::SvgIconColorer::IconSubstitutions colorSubs = {
            { QnIcon::Normal, {{ background, "dark7" }}},
            { QnIcon::Active, {{ background, "red_d1" }}},
            { QnIcon::Pressed, {{ background, "red_d1" }}}};
        QIcon icon = qnSkin->icon("titlebar/window_close.svg", "", nullptr, colorSubs);
        d->systemLayout->addWidget(newActionButton(menu::ExitAction, icon));
    }
    d->widgetsToTransfer << d->layoutBar;
    d->tabLayout->addWidget(d->layoutBar);

    d->widgetsToTransfer << d->newTabButton;
    d->tabLayout->addWidget(d->newTabButton);

    auto line = newVLine(5, "dark12");
    d->widgetsToTransfer << line;
    d->tabLayout->addWidget(line);

    d->widgetsToTransfer << d->currentLayoutsButton;
    d->tabLayout->addWidget(d->currentLayoutsButton);

    line = newVLine(5, "dark12");
    d->widgetsToTransfer << line;
    d->tabLayout->addWidget(line);
    d->tabLayout->addStretch(1);

    d->resizer = new QnMainWindowTitleBarResizerWidget(this);
    connect(d->resizer, &QnMainWindowTitleBarResizerWidget::collapsed, this,
        &QnMainWindowTitleBarWidget::collapse);
    connect(d->resizer, &QnMainWindowTitleBarResizerWidget::expanded, this,
        &QnMainWindowTitleBarWidget::expand);

    installEventHandler({this}, {QEvent::Resize, QEvent::Move}, this,
        [this]()
        {
            Q_D(QnMainWindowTitleBarWidget);
            d->resizer->setGeometry(0, height() - kResizerHeight, width(), kResizerHeight);
            emit geometryChanged();
        });
}

void QnMainWindowTitleBarWidget::initLayoutsOnlyTabBar()
{
    Q_D(QnMainWindowTitleBarWidget);

    /* Layout for window buttons that can be removed from the title bar. */
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    d->layoutBar->setFixedHeight(kSystemBarHeight);

    layout->addWidget(d->mainMenuButton);
    layout->addWidget(newVLine());
    layout->addWidget(d->layoutBar);
    layout->addWidget(newVLine());

    layout->addWidget(d->newTabButton);
    layout->addWidget(d->currentLayoutsButton);
    layout->addStretch(1);
    layout->addSpacing(80);
    layout->addWidget(newVLine());
    layout->addWidget(d->cloudPanel);
    layout->addWidget(newVLine());
    if (auto indicator = newRecordingIndicator(kControlButtonSize))
        layout->addWidget(indicator);
#ifdef ENABLE_LOGIN_TO_ANOTHER_SYSTEM_BUTTON
    layout->addWidget(newActionButton(
        menu::OpenLoginDialogAction,
        HelpTopic::Id::Login,
        kControlButtonSize));
#else
    layout->addSpacing(8);
#endif
    layout->addWidget(newActionButton(
        menu::WhatsThisAction,
        HelpTopic::Id::MainWindow_ContextHelp,
        kControlButtonSize));
    layout->addWidget(newVLine("dark8", "dark6"));
    layout->addWidget(newActionButton(
        menu::MinimizeAction,
        kControlButtonSize));
    layout->addWidget(newVLine("dark8", "dark6"));
    layout->addWidget(newActionButton(
        menu::EffectiveMaximizeAction,
        HelpTopic::Id::MainWindow_Fullscreen,
        kControlButtonSize));
    layout->addWidget(newVLine("dark8", "dark6"));
    {
        const QColor background = "#212A2F";
        const core::SvgIconColorer::IconSubstitutions colorSubs = {
            { QnIcon::Normal, {{ background, "dark7" }}},
            { QnIcon::Active, {{ background, "red_d1" }}},
            { QnIcon::Pressed, {{ background, "red_d1" }}}};
        QIcon icon = qnSkin->icon("titlebar/window_close.svg",
            /*checkedName*/ "",
            /*suffixes*/ nullptr,
            /*svgColorSubstitutions*/ colorSubs);
        layout->addWidget(newActionButton(menu::ExitAction, icon));
    }
}

void QnMainWindowTitleBarWidget::collapse()
{
    Q_D(QnMainWindowTitleBarWidget);
    if (!d->expanded)
        return;

    d->expanded = false;
    int insertPosition = d->systemLayout->indexOf(d->systemBar);
    d->systemBar->hide();
    for (auto widget: d->widgetsToTransfer)
    {
        d->systemLayout->insertWidget(insertPosition++, widget);
        widget->setFixedHeight(kSystemBarHeight);
        if (auto line = qobject_cast<QFrame*>(widget))
        {
            line->setContentsMargins(kSingleLevelVLineContentMargins);
            setPaletteColor(line, QPalette::Shadow, core::colorTheme()->color("dark8"));
        }
    }
    d->layoutBar->setSingleLevelContentMargins();
    d->layoutBar->setSingleLevelPalette();
    setFixedHeight(kSystemBarHeight);
    emit toggled(false);
}

void QnMainWindowTitleBarWidget::expand()
{
    Q_D(QnMainWindowTitleBarWidget);
    if (d->expanded)
        return;

    d->expanded = true;
    int insertPosition = 0;
    for (auto widget: d->widgetsToTransfer)
    {
        d->tabLayout->insertWidget(insertPosition++, widget);
        widget->setFixedHeight(kLayoutBarHeight);
        if (auto line = qobject_cast<QFrame*>(widget))
        {
            line->setContentsMargins(kDoubleLevelVLineContentMargins);
            setPaletteColor(line, QPalette::Shadow, core::colorTheme()->color("dark12"));
        }
        if (d->homeTabActive)
            widget->hide();
    }
    d->systemBar->show();
    d->layoutBar->setDoubleLevelContentMargins();
    d->layoutBar->setDoubleLevelPalette();
    if (!d->homeTabActive)
        setFixedHeight(kFullTitleBarHeight);
    emit toggled(true);
}

QnMainWindowTitleBarResizerWidget::QnMainWindowTitleBarResizerWidget(
    QnMainWindowTitleBarWidget* parent)
    :
    base_type(parent)
{
    setCursor(Qt::SplitVCursor);
}

void QnMainWindowTitleBarResizerWidget::mousePressEvent(QMouseEvent*)
{
    if (isEnabled())
        m_isBeingMoved = true;
}

void QnMainWindowTitleBarResizerWidget::mouseReleaseEvent(QMouseEvent*)
{
    m_isBeingMoved = false;
}

void QnMainWindowTitleBarResizerWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isBeingMoved)
    {
        const int y = mapToParent(event->pos()).y();
        auto parent = qobject_cast<QnMainWindowTitleBarWidget*>(this->parent());
        if (y <= parent->kSystemBarHeight && parent->isExpanded())
            emit collapsed();
        else if (y >= parent->kFullTitleBarHeight && !parent->isExpanded())
            emit expanded();
    }
}
