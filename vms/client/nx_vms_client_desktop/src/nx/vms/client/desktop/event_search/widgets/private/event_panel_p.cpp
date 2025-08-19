// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_panel_p.h"

#include <chrono>

#include <QtCore/QModelIndex>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QWindow>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/camera_resource.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/animated_compact_tab_widget.h>
#include <nx/vms/client/desktop/common/widgets/compact_tab_bar.h>
#include <nx/vms/client/desktop/common/widgets/text_edit_label.h>
#include <nx/vms/client/desktop/event_search/synchronizers/analytics_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/synchronizers/bookmark_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/synchronizers/motion_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/utils/call_alarm_manager.h>
#include <nx/vms/client/desktop/event_search/widgets/analytics_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/bookmark_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/event_search/widgets/notification_counter_label.h>
#include <nx/vms/client/desktop/event_search/widgets/notification_list_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/overlappable_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/private/notification_bell_widget_p.h>
#include <nx/vms/client/desktop/event_search/widgets/simple_motion_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/vms_event_search_widget.h>
#include <nx/vms/client/desktop/image_providers/multi_image_provider.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/widgets/thumbnail_tooltip.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/animation/variant_animator.h>
#include <ui/animation/widget_opacity_animator.h>
#include <ui/common/notification_levels.h>
#include <ui/processors/hover_processor.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

namespace {

const QString kEventPanelStorageKey = "eventPanel";

const QString kBookmarksPreviewKey = "bookmarksPreview";
const QString kEventsPreviewKey = "eventsPreview";
const QString kMotionPreviewKey = "motionPreview";
const QString kObjectsPreviewKey = "objectsPreview";
const QString kObjectsInformation = "objectsInformation";
const QString kTabIndexKey = "tabIndex";

static const QSize kToolTipMaxThumbnailSize{240, 180};
static constexpr int kMaxMultiThumbnailCount = 5;
static constexpr int kMultiThumbnailSpacing = 1;

const core::SvgIconColorer::ThemeSubstitutions kTabIconSubstitutions = {
    {QnIcon::Disabled, {.primary = "dark14", .secondary = "red"}},
    {QnIcon::Selected, {.primary = "light4", .secondary = "red"}},
    {QnIcon::Active, {.primary = "brand_core", .secondary = "red"}},
    {QnIcon::Normal, {.primary = "light10", .secondary = "red"}}};

NX_DECLARE_COLORIZED_ICON(kObjectIcon, "20x20/Solid/object.svg", kTabIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(kBookmarkIcon, "20x20/Solid/bookmark.svg", kTabIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(kEventIcon, "20x20/Solid/event.svg", kTabIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(kMotionIcon, "20x20/Solid/motion.svg", kTabIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(kNotificationIcon, "20x20/Solid/notification.svg", kTabIconSubstitutions)

QRect globalGeometry(QWidget* widget)
{
    return widget
        ? QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size())
        : QRect();
}

} // namespace

class EventPanel::Private::StateDelegate: public ClientStateDelegate
{
public:
    StateDelegate(EventPanel::Private* p): m_panel(p)
    {
    }

    virtual bool loadState(
        const DelegateState& state,
        SubstateFlags flags,
        const StartupParameters& /*params*/) override
    {
        if (!flags.testFlag(ClientStateDelegate::Substate::systemIndependentParameters))
            return false;

        m_panel->m_bookmarksTab->searchWidget()->setPreviewToggled(
            state.value(kBookmarksPreviewKey).toBool(true));
        m_panel->m_vmsEventsTab->searchWidget()->setPreviewToggled(
            state.value(kEventsPreviewKey).toBool(true));
        m_panel->m_motionTab->setPreviewToggled(state.value(kMotionPreviewKey).toBool(true));
        m_panel->m_analyticsTab->searchWidget()->setPreviewToggled(
            state.value(kObjectsPreviewKey).toBool(true));
        m_panel->m_analyticsTab->searchWidget()->setFooterToggled(
            state.value(kObjectsInformation).toBool(true));
        const auto tab = static_cast<Tab>(state.value(kTabIndexKey).toInt(0));
        m_panel->setCurrentTab(tab);

        reportStatistics("right_panel_active_tab", tab);
        reportStatistics(
            "right_panel_event_thumbnails",
            m_panel->m_vmsEventsTab->searchWidget()->previewToggled());
        reportStatistics(
            "right_panel_motion_thumbnails",
            m_panel->m_motionTab->previewToggled());
        reportStatistics(
            "right_panel_bookmark_thumbnails",
            m_panel->m_bookmarksTab->searchWidget()->previewToggled());
        reportStatistics(
            "right_panel_analytics_thumbnails",
            m_panel->m_analyticsTab->searchWidget()->previewToggled());
        reportStatistics(
            "right_panel_analytics_information",
            m_panel->m_analyticsTab->searchWidget()->footerToggled());

        return true;
    }

    virtual void saveState(DelegateState* state, SubstateFlags flags) override
    {
        if (flags.testFlag(ClientStateDelegate::Substate::systemIndependentParameters))
        {
            DelegateState result;
            result[kBookmarksPreviewKey] =
                m_panel->m_bookmarksTab->searchWidget()->previewToggled();
            result[kEventsPreviewKey] = m_panel->m_vmsEventsTab->searchWidget()->previewToggled();
            result[kMotionPreviewKey] = m_panel->m_motionTab->previewToggled();
            result[kObjectsPreviewKey] =
                m_panel->m_analyticsTab->searchWidget()->previewToggled();
            result[kObjectsInformation] =
                m_panel->m_analyticsTab->searchWidget()->footerToggled();
            result[kTabIndexKey] = static_cast<int>(m_panel->currentTab());
            *state = result;
        }
    }

private:
    EventPanel::Private* m_panel;
};

// ------------------------------------------------------------------------------------------------
// EventPanel::Private

EventPanel::Private::Private(EventPanel* q):
    QObject(q),
    WindowContextAware(q),
    q(q),
    m_callAlarmManager(new CallAlarmManager(windowContext())),
    m_tooltip(new ThumbnailTooltip(q->windowContext()))
{
    auto compactTabBar = new CompactTabBar();
    compactTabBar->setCustomTabEnabledFunction(
        [this](int index)
        {
            if (workbench()->currentLayout()->resource()->isCrossSystem()
                && (index == static_cast<int>(Tab::bookmarks)
                    || index == static_cast<int>(Tab::vmsEvents)))
            {
                return false;
            }

            return m_tabs->isTabEnabled(index);
        });

    m_tabs = new AnimatedCompactTabWidget(compactTabBar, q);
    m_tabs->setStyleSheet("QTabWidget::tab-bar { left: -1px; } ");

    const auto context = windowContext();
    m_notificationsTab = new NotificationListWidget(m_tabs);
    m_counterLabel = new NotificationCounterLabel(m_tabs->tabBar());
    m_motionTab = new SimpleMotionSearchWidget(context, m_tabs);
    m_bookmarksTab = new OverlappableSearchWidget(new BookmarkSearchWidget(context), m_tabs);
    m_vmsEventsTab = new OverlappableSearchWidget(new VmsEventSearchWidget(context), m_tabs);
    m_analyticsTab = new OverlappableSearchWidget(new AnalyticsSearchWidget(context), m_tabs);

    m_synchronizers = {
        {m_motionTab, m_motionTab, new MotionSearchSynchronizer(
            context, m_motionTab->commonSetup(), m_motionTab->motionModel(), this)},
        {m_bookmarksTab, m_bookmarksTab->searchWidget(), new BookmarkSearchSynchronizer(
            context, m_bookmarksTab->searchWidget()->commonSetup(), this)},
        {m_analyticsTab, m_analyticsTab->searchWidget(), new AnalyticsSearchSynchronizer(
            context,
            m_analyticsTab->searchWidget()->commonSetup(),
            static_cast<AnalyticsSearchWidget*>(m_analyticsTab->searchWidget())->analyticsSetup(),
            this)}};

    m_tabIds = {
        {m_notificationsTab, Tab::notifications},
        {m_motionTab, Tab::motion},
        {m_bookmarksTab, Tab::bookmarks},
        {m_vmsEventsTab, Tab::vmsEvents},
        {m_analyticsTab, Tab::analytics}};

    auto layout = new QVBoxLayout(q);
    layout->setContentsMargins(QMargins());

    layout->addWidget(m_tabs);

    // Initially all tab widgets must be hidden.
    for (const auto tab: m_tabIds.keys())
        tab->hide();

    static constexpr int kTabBarShift = 10;
    m_tabs->setProperty(style::Properties::kTabBarShift, kTabBarShift);
    setTabShape(m_tabs->tabBar(), style::TabShape::Compact);

    for (const auto& data: m_synchronizers)
    {
        connect(data.synchronizer, &AbstractSearchSynchronizer::activeChanged, this,
            [this, &data](bool isActive)
            {
                if (!data.searchWidget->isAllowed())
                    return;

                if (isActive)
                    m_tabs->setCurrentWidget(data.tab);
                else if (m_tabs->currentWidget() == data.tab)
                    setCurrentTab(Tab::notifications);
            });
    }

    connect(m_tabs, &QTabWidget::currentChanged, this,
        [this]
        {
            const auto currentTab = m_tabs->currentWidget();

            for (const auto& data: m_synchronizers)
                data.synchronizer->setActive(m_tabs->currentWidget() == data.tab);

            m_callAlarmManager->setActive(currentTab != m_notificationsTab);

            emit this->q->currentTabChanged(
                m_tabIds.value(currentTab), EventPanel::QPrivateSignal());
        });

    connect(action(menu::NotificationsTabAction), &QAction::triggered, this,
        [this] { setCurrentTab(Tab::notifications); });

    connect(action(menu::MotionTabAction), &QAction::triggered, this,
        [this] { setCurrentTab(Tab::motion); });

    connect(action(menu::SwitchMotionTabAction), &QAction::triggered, this,
        [this]
        {
            if (currentTab() == Tab::motion)
                setCurrentTab(Tab::notifications);
            else
                setCurrentTab(Tab::motion);
        });

    connect(action(menu::BookmarksTabAction), &QAction::triggered, this,
        [this] { setCurrentTab(Tab::bookmarks); });

    connect(action(menu::EventsTabAction), &QAction::triggered, this,
        [this] { setCurrentTab(Tab::vmsEvents); });

    connect(action(menu::ObjectsTabAction), &QAction::triggered, this,
        [this] { setCurrentTab(Tab::analytics); });

    connect(action(menu::ResetCurrenTabFiltersAndSelectNotificationsTabAction), &QAction::triggered, this,
        [this]
        {
            auto widget = qobject_cast<AbstractSearchWidget*>(m_tabs->currentWidget());
            if (!widget)
            {
                if (auto overlappable = qobject_cast<OverlappableSearchWidget*>(m_tabs->currentWidget()))
                    widget = overlappable->searchWidget();
            }

            if (widget)
            {
                widget->resetFilters();
                setCurrentTab(Tab::notifications);
            }
        });

    menu()->trigger(menu::ResetCurrenTabFiltersAndSelectNotificationsTabAction);

    connect(m_notificationsTab, &NotificationListWidget::unreadCountChanged,
        this, &Private::updateUnreadCounter);

    m_counterLabel->hide();

    connect(m_tabs->tabBar(), &QTabBar::tabBarDoubleClicked, this,
        [this](int index)
        {
            if (m_tabs->currentIndex() != index)
                return;

            if (auto searchWidget = currentSearchWidget())
                searchWidget->goToLive();
        });

    using Tabs = std::initializer_list<AbstractSearchWidget*>;
    for (const auto tab: Tabs{
        m_vmsEventsTab->searchWidget(),
        m_motionTab,
        m_bookmarksTab->searchWidget(),
        m_analyticsTab->searchWidget()})
    {
        m_connections << connect(tab, &AbstractSearchWidget::tileHovered,
            this, &EventPanel::Private::at_eventTileHovered);

        connect(tab, &AbstractSearchWidget::allowanceChanged,
            this, &Private::rebuildTabs);
    }

    connect(
        workbench(),
        &Workbench::currentLayoutChanged,
        this,
        [this]
        {
            const auto appearance = workbench()->currentLayout()->resource()->isCrossSystem()
                ? OverlappableSearchWidget::Appearance::overlay
                : OverlappableSearchWidget::Appearance::searchWidget;

            m_bookmarksTab->setAppearance(
                nx::vms::client::core::ini().allowCslBookmarkSearch
                    ? OverlappableSearchWidget::Appearance::searchWidget
                    : appearance);

            m_analyticsTab->setAppearance(
                nx::vms::client::core::ini().allowCslObjectSearch
                    ? OverlappableSearchWidget::Appearance::searchWidget
                    : appearance);

            // Events are not supported for the cross systems at the moment.
            m_vmsEventsTab->setAppearance(appearance);
        });

    rebuildTabs();

    m_connections << connect(m_notificationsTab, &NotificationListWidget::tileHovered,
        this, &EventPanel::Private::at_eventTileHovered);

    m_connections << connect(m_notificationsTab, &NotificationListWidget::unreadCountChanged,
        q, &EventPanel::unreadCountChanged);

    q->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(q, &EventPanel::customContextMenuRequested,
        this, &EventPanel::Private::showContextMenu);

    appContext()->clientStateHandler()->registerDelegate(
        kEventPanelStorageKey, std::make_unique<StateDelegate>(this));

    connect(m_callAlarmManager.get(), &CallAlarmManager::ringingChanged, this,
        [this]()
        {
            if (m_notificationBellWidget)
                m_notificationBellWidget->setAlarmState(m_callAlarmManager->isRinging());
        });
}

EventPanel::Private::~Private()
{
    appContext()->clientStateHandler()->unregisterDelegate(kEventPanelStorageKey);
}

EventPanel::Tab EventPanel::Private::currentTab() const
{
    return m_tabIds.value(m_tabs->currentWidget());
}

bool EventPanel::Private::setCurrentTab(Tab tab)
{
    const int index = m_tabs->indexOf(m_tabIds.key(tab));
    if (index < 0)
        return false;

    m_tabs->setCurrentIndex(index);

    return true;
}

AbstractSearchWidget* EventPanel::Private::currentSearchWidget() const
{
    if (auto widget = qobject_cast<AbstractSearchWidget*>(m_tabs->currentWidget()))
        return widget;

    if (auto overlappable = qobject_cast<OverlappableSearchWidget*>(m_tabs->currentWidget()))
        return overlappable->searchWidget();

    return nullptr;
}

void EventPanel::Private::rebuildTabs()
{
    const auto selectedTab = m_tabs->currentWidget();
    int updatingVisibleTabIndex = 0;

    const auto updateTab =
        [this, selectedTab, &updatingVisibleTabIndex](
            QWidget* tab,
            bool showTab,
            const QIcon& icon,
            const QString& text,
            QWidget* tabButton = nullptr)
        {
            const int tabIndexBeforeRebuild = m_tabs->indexOf(tab);
            const bool tabWasVisibleBeforeRebuild = tabIndexBeforeRebuild >= 0;

            NX_ASSERT(tabIndexBeforeRebuild < 0
                || tabIndexBeforeRebuild == updatingVisibleTabIndex);

            if (showTab)
            {
                if (!tabWasVisibleBeforeRebuild)
                {
                    // Add tab.
                    if (tab == m_notificationsTab)
                    {
                        NX_ASSERT(!m_notificationBellWidget);
                        m_notificationBellWidget = new NotificationBellWidget(q);

                        m_tabs->insertIconWidgetTab(
                            updatingVisibleTabIndex,
                            tab,
                            m_notificationBellWidget,
                            text.toUpper());
                    }
                    else
                    {
                        m_tabs->insertTab(updatingVisibleTabIndex, tab, icon, text.toUpper());
                    }
                    m_tabs->setTabToolTip(updatingVisibleTabIndex, text);

                    if (tabButton)
                        m_tabs->tabBar()->setTabButton(updatingVisibleTabIndex, QTabBar::LeftSide, tabButton);
                }

                ++updatingVisibleTabIndex;
            }
            else //< Hide tab.
            {
                if (selectedTab == tab) //< If True, selected tab is not visible anymore.
                    m_tabs->setCurrentIndex(0); //< Show Notifications tab by default.

                if (tabWasVisibleBeforeRebuild)
                {
                    m_tabs->removeTab(updatingVisibleTabIndex);
                    tab->hide();
                }
            }
        };

    const auto resource = navigator()->currentResource();

    updateTab(m_notificationsTab,
        true, // Notifications tab is always visible.
        qnSkin->icon(kNotificationIcon),
        tr("Notifications", "Notifications tab title"));

    updateTab(m_motionTab,
        m_motionTab->isAllowed(),
        qnSkin->icon(kMotionIcon),
        tr("Motion", "Motion tab title"));

    updateTab(m_bookmarksTab,
        m_bookmarksTab->searchWidget()->isAllowed(),
        qnSkin->icon(kBookmarkIcon),
        tr("Bookmarks", "Bookmarks tab title"));

    updateTab(m_vmsEventsTab,
        m_vmsEventsTab->searchWidget()->isAllowed(),
        qnSkin->icon(kEventIcon),
        tr("Events", "Events tab title"));

    updateTab(m_analyticsTab,
        m_analyticsTab->searchWidget()->isAllowed(),
        qnSkin->icon(kObjectIcon),
        tr("Objects", "Analytics tab title"));

    // Make sure the number of notifications doesn't get obscured by the bell icon.
    m_counterLabel->raise();
}

void EventPanel::Private::updateUnreadCounter(int count, QnNotificationLevel::Value importance)
{
    m_counterLabel->setVisible(count > 0);
    if (count == 0)
        return;

    const auto text = (count > 99) ? QString("99+") : QString::number(count);
    const auto color = QnNotificationLevel::notificationTextColor(importance);

    m_counterLabel->setText(text);
    m_counterLabel->setBackgroundColor(color);

    const auto width = m_counterLabel->minimumSizeHint().width();

    static constexpr int kTopMargin = 6;
    static constexpr int kRightBoundaryPosition = 29;
    m_counterLabel->setGeometry(kRightBoundaryPosition - width, kTopMargin,
        width, m_counterLabel->minimumHeight());
}

void EventPanel::Private::showContextMenu(const QPoint& pos)
{
    QMenu contextMenu;
    const auto actions = {
        menu::OpenEventLogAction,
        menu::OpenVmsRulesDialogAction};

    for (const auto actionId: actions)
    {
        if (menu()->canTrigger(actionId))
            contextMenu.addAction(action(actionId));
    }

    contextMenu.exec(QnHiDpiWorkarounds::safeMapToGlobal(q, pos));
}

void EventPanel::Private::at_eventTileHovered(const QModelIndex& index, EventTile* tile)
{
    hoveredTilePositionWatcher.reset();

    if (!tile || !index.isValid() || (Qt::NoButton != QApplication::mouseButtons()))
    {
        m_tooltip->hide();
        return;
    }

    auto multiImageProvider = this->multiImageProvider(index);
    const auto imageProvider = multiImageProvider ? multiImageProvider.get() : tile->imageProvider();

    auto text = tile->toolTip();
    if (text.isEmpty() && imageProvider)
        text = tile->title();
    if (text.isEmpty())
    {
        m_tooltip->hide();
        return;
    }

    m_lastHoveredTile = tile;

    m_tooltip->setText(text);
    m_tooltip->setImageProvider(imageProvider);
    m_tooltip->setHighlightRect(tile->previewHighlightRect());
    m_tooltip->setAttributes(tile->attributeList());
    m_tooltip->adjustMaximumContentSize(index);

    const auto parentRect = globalGeometry(mainWindowWidget());
    const auto panelRect = globalGeometry(q);
    m_tooltip->setEnclosingRect(
        QRect(parentRect.left(), panelRect.top(), parentRect.width(), panelRect.height()));

    m_tooltip->setTarget(globalGeometry(m_lastHoveredTile));
    m_tooltip->show();

    if (multiImageProvider)
    {
        multiImageProvider->loadAsync();
        multiImageProvider.release()->setParent(m_tooltip.get());
    }

    hoveredTilePositionWatcher.reset(
        installEventHandler(m_lastHoveredTile, {QEvent::Resize, QEvent::Move}, this,
            [this](QObject* /*watched*/, QEvent* event)
            {
                if (event->type() == QEvent::Move
                    && m_tooltip->targetRect().topLeft()
                        != globalGeometry(m_lastHoveredTile).topLeft())
                {
                    m_tooltip->suppress(/*immediately*/ true);
                }

                m_tooltip->setTarget(globalGeometry(m_lastHoveredTile));
            }));
}

std::unique_ptr<MultiImageProvider> EventPanel::Private::multiImageProvider(
    const QModelIndex& index) const
{
    const auto previewTimeData = index.data(core::PreviewTimeRole);
    if (previewTimeData.isNull())
        return {};

    const auto previewTimeMs =
        duration_cast<milliseconds>(previewTimeData.value<microseconds>());

    const auto requiredPermission = previewTimeMs > 0ms
        ? Qn::ViewFootagePermission
        : Qn::ViewLivePermission;

    const auto cameras = index.data(core::ResourceListRole).value<QnResourceList>()
        .filtered<QnVirtualCameraResource>(
            [requiredPermission](const QnVirtualCameraResourcePtr& camera)
            {
                if (!NX_ASSERT(camera) || camera->hasFlags(Qn::ResourceFlag::fake))
                    return false;

                // Assuming the rights were checked before sending the notification.
                if (camera->hasFlags(Qn::ResourceFlag::cross_system))
                    return true;

                return ResourceAccessManager::hasPermissions(camera, requiredPermission);
            });

    if (cameras.size() < 2)
        return {};

    const bool precisePreview = index.data(core::ForcePrecisePreviewRole).toBool();
    const auto streamSelectionMode = index.data(core::PreviewStreamSelectionRole)
        .value<nx::api::CameraImageRequest::StreamSelectionMode>();

    MultiImageProvider::Providers providers;

    nx::api::CameraImageRequest request;
    request.size = QSize(kToolTipMaxThumbnailSize.width(), 0);
    request.format = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::auto_;
    request.streamSelectionMode = streamSelectionMode;
    request.timestampMs =
        previewTimeMs.count() > 0 ? previewTimeMs : nx::api::ImageRequest::kLatestThumbnail;
    request.roundMethod = precisePreview
        ? nx::api::ImageRequest::RoundMethod::precise
        : nx::api::ImageRequest::RoundMethod::iFrameAfter;

    for (const auto& camera: cameras)
    {
        if (providers.size() >= kMaxMultiThumbnailCount)
            break;

        request.camera = camera;
        providers.emplace_back(new core::CameraThumbnailProvider(request));
    }

    return std::make_unique<MultiImageProvider>(
        std::move(providers), Qt::Vertical, kMultiThumbnailSpacing);
}

} // namespace nx::vms::client::desktop
