// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "main_window_title_bar_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QDragMoveEvent>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QMenu>

#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/app_info.h>
#include <nx/utils/math/math.h>
#include <nx/utils/virtual_property.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/desktop/common/widgets/screen_recording_indicator.h>
#include <nx/vms/client/desktop/common/widgets/tool_button.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_tab_bar/home_tab_button.h>
#include <nx/vms/client/desktop/system_tab_bar/system_tab_bar.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <ui/common/palette.h>
#include <ui/widgets/cloud_status_panel.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/widgets/system_tab_bar_state_handler.h>
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

struct LayoutNavigationStyle
{
    int height;
    QString background;
    QMargins lineMargins;
    QString lineColor;

    bool operator==(const LayoutNavigationStyle&) const = default;
};

static const LayoutNavigationStyle kExpandedLayoutNavigationLineStyle{
    .height = QnMainWindowTitleBarWidget::kLayoutBarHeight,
    .background = "dark10",
    .lineMargins{0, 5, 0, 5},
    .lineColor = "dark12",
};

static const LayoutNavigationStyle kCollapsedLayoutNavigationStyle{
    .height = QnMainWindowTitleBarWidget::kSystemBarHeight,
    .background = "dark7",
    .lineMargins{},
    .lineColor = "dark8",
};

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kMainMenuTheme = {
    {QnIcon::Normal, {.primary = "light4"}}};
NX_DECLARE_COLORIZED_ICON(kMainMenuIcon, "40x32/Solid/main_menu_2.svg", kMainMenuTheme);

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
    QVBoxLayout* mainLayout = nullptr;
    QHBoxLayout* topInnerLayout = nullptr;
    QHBoxLayout* bottomLayout = nullptr;

    ToolButton* mainMenuButton = nullptr;
    QWidget* systemNavigationWidget = nullptr;
    SystemTabBar* systemTabBar = nullptr;
    HomeTabButton* homeTabButton = nullptr;
    QWidget* layoutNavigationWidget = nullptr;
    QnLayoutTabBar* layoutTabBar = nullptr;
    ToolButton* newLayoutButton = nullptr;
    ToolButton* currentLayoutsButton = nullptr;
    QnCloudStatusPanel* cloudPanel = nullptr;
    std::unique_ptr<MimeData> mimeData;
    bool skipDoubleClickFlag = false;
    QnMainWindowTitleBarResizerWidget* resizer = nullptr;

    nx::utils::VirtualProperty<LayoutNavigationStyle> layoutNavigationLineStyle{
        kCollapsedLayoutNavigationStyle};

    QSharedPointer<QnMainWindowTitleBarWidget::Store> store;

    /** There are some rare scenarios with looping menus. */
    bool isMenuOpened = false;

    QSharedPointer<QMenu> mainMenuHolder;
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
        qnSkin->icon(kMainMenuIcon),
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

    d->cloudPanel = new QnCloudStatusPanel(windowContext, this);
    d->cloudPanel->setFocusPolicy(Qt::NoFocus);

    initLayoutNavigation();
    initControls();

    if (ini().enableMultiSystemTabBar)
        initSystemNavigation();
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
    return d->layoutTabBar;
}

void QnMainWindowTitleBarWidget::setStateStore(QSharedPointer<Store> store)
{
    Q_D(QnMainWindowTitleBarWidget);
    d->store = store;

    connect(this,
        &QnMainWindowTitleBarWidget::toggled,
        store.get(),
        &MainWindowTitleBarStateStore::setExpanded);

    connect(store.get(),
        &MainWindowTitleBarStateStore::stateChanged,
        this,
        &QnMainWindowTitleBarWidget::handleStateChange);

    d->systemTabBar->setStateStore(store);
    d->homeTabButton->setStateStore(store);

    handleStateChange(d->store->state());
}

void QnMainWindowTitleBarWidget::setTabBarStuffVisible(bool visible)
{
    Q_D(const QnMainWindowTitleBarWidget);
    d->layoutTabBar->setVisible(visible);
    d->newLayoutButton->setVisible(visible);
    d->currentLayoutsButton->setVisible(visible);
    action(menu::OpenNewTabAction)->setEnabled(visible);
}

bool QnMainWindowTitleBarWidget::isExpanded() const
{
    Q_D(const QnMainWindowTitleBarWidget);
    return d->store && d->store->state().expanded;
}

void QnMainWindowTitleBarWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    base_type::mouseDoubleClickEvent(event);

#ifndef Q_OS_MACOS
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

    // Check whether mime data is made by the same user from the same system.
    if (!d->mimeData->allowedInWindowContext(windowContext()))
        return;

    if (d->mimeData->isEmpty())
        return;

    event->acceptProposedAction();
}

void QnMainWindowTitleBarWidget::dragMoveEvent(QDragMoveEvent* event)
{
    Q_D(QnMainWindowTitleBarWidget);
    if (d->mimeData->isEmpty())
        return;

    const int x = event->position().toPoint().x();
    if (!qBetween(d->layoutTabBar->pos().x(), x, d->cloudPanel->pos().x()))
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

    // Check whether mime data is made by the same user from the same system.
    if (!d->mimeData->allowedInWindowContext(windowContext()))
        return;

    if (d->mimeData->isEmpty())
        return;

    const auto systemContext = system();
    if (!systemContext)
        return;

    const auto showreels = systemContext->showreelManager()->showreels(d->mimeData->entities());
    for (const auto& showreel: showreels)
        menu()->trigger(menu::ReviewShowreelAction, {core::UuidRole, showreel.id});

    systemContext->resourcePool()->addNewResources(d->mimeData->resources());

    menu()->triggerIfPossible(menu::OpenInNewTabAction, d->mimeData->resources());

    d->mimeData.reset();
    event->acceptProposedAction();
}

ToolButton* QnMainWindowTitleBarWidget::newActionButton(
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

void QnMainWindowTitleBarWidget::initSystemNavigation()
{
    Q_D(QnMainWindowTitleBarWidget);

    d->systemTabBar = new SystemTabBar(this);
    d->systemTabBar->setFocusPolicy(Qt::NoFocus);

    d->homeTabButton = new HomeTabButton(this);

    d->systemNavigationWidget = new QWidget(this);
    auto layout = new QHBoxLayout(d->systemNavigationWidget);
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->addWidget(d->systemTabBar, 1);
    layout->addWidget(d->homeTabButton);
    layout->setAlignment(Qt::AlignLeft);

    d->topInnerLayout->addWidget(d->systemNavigationWidget);
    d->systemNavigationWidget->hide();
}

void QnMainWindowTitleBarWidget::initLayoutNavigation()
{
    Q_D(QnMainWindowTitleBarWidget);

    d->layoutTabBar = new QnLayoutTabBar(windowContext(), this);
    d->layoutTabBar->setFocusPolicy(Qt::NoFocus);
    connect(d->layoutTabBar, &QnLayoutTabBar::tabCloseRequested, d,
        &QnMainWindowTitleBarWidgetPrivate::setSkipDoubleClick);

    const QString styleSheet = QString("QToolButton:hover { background-color: %1 }")
        .arg(core::colorTheme()->color("dark8").name());
    d->newLayoutButton = newActionButton(
        menu::OpenNewTabAction,
        HelpTopic::Id::MainWindow_TitleBar_NewLayout,
        kTabBarButtonSize);
    d->newLayoutButton->setStyleSheet(styleSheet);

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

    d->layoutNavigationWidget = new QWidget(this);
    d->layoutNavigationWidget->setAutoFillBackground(true);

    const auto updateStyle =
        [this]()
        {
            Q_D(QnMainWindowTitleBarWidget);
            const LayoutNavigationStyle& style = d->layoutNavigationLineStyle.value();
            d->layoutNavigationWidget->setFixedHeight(style.height);
            for (QObject* child: d->layoutNavigationWidget->children())
            {
                if (const auto widget = qobject_cast<QWidget*>(child))
                    widget->setFixedHeight(style.height);
            }
            setPaletteColor(d->layoutNavigationWidget,
                QPalette::Window,
                core::colorTheme()->color(style.background));
        };

    connect(&d->layoutNavigationLineStyle,
        &nx::utils::VirtualPropertyBase::valueChanged,
        d->layoutNavigationWidget,
        updateStyle);

    auto layout = new QHBoxLayout(d->layoutNavigationWidget);
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto addVLine =
        [this, layout]()
        {
            Q_D(QnMainWindowTitleBarWidget);

            QFrame* line = newVLine();
            layout->addWidget(line);

            const auto updateStyle =
                [this, line]()
                {
                    Q_D(QnMainWindowTitleBarWidget);

                    const LayoutNavigationStyle& style = d->layoutNavigationLineStyle.value();

                    line->setContentsMargins(style.lineMargins);
                    setPaletteColor(
                        line, QPalette::Shadow, core::colorTheme()->color(style.lineColor));
                };

            connect(&d->layoutNavigationLineStyle,
                &nx::utils::VirtualPropertyBase::valueChanged,
                line,
                updateStyle);

            updateStyle();
        };

    layout->addWidget(d->layoutTabBar);
    layout->addWidget(d->newLayoutButton);
    addVLine();
    layout->addWidget(d->currentLayoutsButton);
    addVLine();
    layout->addStretch(1);

    updateStyle();
}

void QnMainWindowTitleBarWidget::initControls()
{
    Q_D(QnMainWindowTitleBarWidget);

    d->topInnerLayout = new QHBoxLayout();
    d->topInnerLayout->setContentsMargins({});
    d->topInnerLayout->setSpacing(0);
    d->topInnerLayout->addWidget(d->layoutNavigationWidget);

    d->bottomLayout = new QHBoxLayout();
    d->bottomLayout->setContentsMargins({});
    d->bottomLayout->setSpacing(0);

    auto topLayout = new QHBoxLayout();
    topLayout->addWidget(d->mainMenuButton);
    topLayout->addWidget(newVLine(0, "dark8"));
    topLayout->addLayout(d->topInnerLayout, 1);
    topLayout->addSpacing(80);
    topLayout->addWidget(newVLine("dark8", "dark6"));
    topLayout->addWidget(d->cloudPanel);
    topLayout->addWidget(newVLine("dark8", "dark6"));
    if (auto indicator = newRecordingIndicator(kControlButtonSize))
    {
        topLayout->addWidget(indicator);
        topLayout->addWidget(newVLine("dark8", "dark6"));
    }
    topLayout->addWidget(newActionButton(
        menu::WhatsThisAction,
        HelpTopic::Id::MainWindow_ContextHelp,
        kControlButtonSize));
    topLayout->addWidget(newVLine("dark8", "dark6"));
    topLayout->addWidget(newActionButton(
        menu::MinimizeAction,
        kControlButtonSize));
    topLayout->addWidget(newVLine("dark8", "dark6"));
    topLayout->addWidget(newActionButton(
        menu::EffectiveMaximizeAction,
        HelpTopic::Id::LaunchingAndClosing,
        kControlButtonSize));
    topLayout->addWidget(newVLine("dark8", "dark6"));
    {
        const QColor background = "#212A2F";
        const QColor cross = "#E1E7EA";
        const core::SvgIconColorer::IconSubstitutions colorSubs = {
            { QnIcon::Normal, {{background, "dark7"}, {cross, "light4"}}},
            { QnIcon::Active, {{background, "red" }, {cross, "light4"}}},
            { QnIcon::Pressed, {{background, "red" }, {cross, "light4"}}}};
        QIcon icon = qnSkin->icon("20x20/Outline/close_main_window.svg", "", nullptr, colorSubs);
        topLayout->addWidget(newActionButton(menu::ExitAction, icon));
    }

    d->mainLayout = new QVBoxLayout(this);
    d->mainLayout->setContentsMargins(0, 0, 0, 0);
    d->mainLayout->setSpacing(0);
    d->mainLayout->addLayout(topLayout);
    d->mainLayout->addLayout(d->bottomLayout);

    d->resizer = new QnMainWindowTitleBarResizerWidget(this);
    connect(d->resizer, &QnMainWindowTitleBarResizerWidget::expanded, this,
        [this]()
        {
            Q_D(QnMainWindowTitleBarWidget);
            if (d->store)
                d->store->setExpanded(true);
        });
    connect(d->resizer, &QnMainWindowTitleBarResizerWidget::collapsed, this,
        [this]()
        {
            Q_D(QnMainWindowTitleBarWidget);
            if (d->store)
                d->store->setExpanded(false);
        });
    d->resizer->hide();

    installEventHandler({this}, {QEvent::Resize, QEvent::Move}, this,
        [this]()
        {
            Q_D(QnMainWindowTitleBarWidget);
            d->resizer->setGeometry(0, height() - kResizerHeight, width(), kResizerHeight);
            if (d->resizer->isVisible())
                d->resizer->raise();
            emit geometryChanged();
        });
}

void QnMainWindowTitleBarWidget::handleStateChange(const State& state)
{
    Q_D(QnMainWindowTitleBarWidget);

    if (state.expanded)
    {
        if (state.layoutNavigationVisible && d->bottomLayout->isEmpty())
        {
            d->layoutNavigationWidget->hide();
            d->topInnerLayout->removeWidget(d->layoutNavigationWidget);
            d->bottomLayout->addWidget(d->layoutNavigationWidget);
        }
        d->layoutTabBar->setDoubleLevelContentMargins();
        d->layoutTabBar->setDoubleLevelPalette();
    }
    else
    {
        if (d->topInnerLayout->isEmpty()
            || d->topInnerLayout->itemAt(0)->widget() != d->layoutNavigationWidget)
        {
            d->layoutNavigationWidget->hide();
            d->bottomLayout->removeWidget(d->layoutNavigationWidget);
            d->topInnerLayout->addWidget(d->layoutNavigationWidget);
        }
        d->layoutTabBar->setSingleLevelContentMargins();
        d->layoutTabBar->setSingleLevelPalette();
    }

    const bool fullHeight = state.expanded && state.layoutNavigationVisible;

    setFixedHeight(fullHeight ? kFullTitleBarHeight : kSystemBarHeight);
    d->layoutNavigationLineStyle.setValue(fullHeight
        ? kExpandedLayoutNavigationLineStyle
        : kCollapsedLayoutNavigationStyle);

    d->layoutNavigationWidget->setVisible(state.layoutNavigationVisible || !state.expanded);
    d->systemNavigationWidget->setVisible(state.expanded);
    d->resizer->setVisible(!state.homeTabActive);
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
