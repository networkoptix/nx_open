#include "event_panel_p.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/camera_resource.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/vms/client/desktop/common/widgets/animated_tab_widget.h>
#include <nx/vms/client/desktop/common/widgets/compact_tab_bar.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/event_search/widgets/motion_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/bookmark_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/event_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/analytics_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/notification_list_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/notification_counter_label.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// EventPanel::Private

EventPanel::Private::Private(EventPanel* q):
    QObject(q),
    QnWorkbenchContextAware(q),
    q(q),
    m_tabs(new AnimatedTabWidget(new CompactTabBar(), q)),
    m_notificationsTab(new NotificationListWidget(m_tabs)),
    m_counterLabel(new NotificationCounterLabel(m_tabs->tabBar())),
    m_motionTab(new MotionSearchWidget(context(), m_tabs)),
    m_bookmarksTab(new BookmarkSearchWidget(context(), m_tabs)),
    m_eventsTab(new EventSearchWidget(context(), m_tabs)),
    m_analyticsTab(new AnalyticsSearchWidget(context(), m_tabs)),
    m_motionSearchSynchronizer(new MotionSearchSynchronizer(this)),
    m_bookmarkSearchSynchronizer(new BookmarkSearchSynchronizer(this)),
    m_analyticsSearchSynchronizer(new AnalyticsSearchSynchronizer(this))
{
    auto layout = new QVBoxLayout(q);
    layout->setContentsMargins(QMargins());
    layout->addWidget(m_tabs);

    static constexpr int kTabBarShift = 10;
    m_tabs->setProperty(style::Properties::kTabBarIndent, kTabBarShift);
    setTabShape(m_tabs->tabBar(), style::TabShape::Compact);

    connect(m_tabs, &QTabWidget::currentChanged, this,
        [this]()
        {
            m_previousTab = m_lastTab;
            m_lastTab = m_tabs->currentWidget();
            NX_VERBOSE(this->q, "Tab changed; previous: %1, current: %2", m_previousTab, m_lastTab);
        });

    q->setAutoFillBackground(false);
    q->setAttribute(Qt::WA_TranslucentBackground);

    connect(m_notificationsTab, &NotificationListWidget::unreadCountChanged,
        this, &Private::updateUnreadCounter);

    connect(m_tabs->tabBar(), &QTabBar::tabBarDoubleClicked, this,
        [this](int index)
        {
            if (m_tabs->currentIndex() != index)
                return;

            if (auto tab = qobject_cast<AbstractSearchWidget*>(m_tabs->currentWidget()))
                tab->goToLive();
        });

    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged,
        this, &Private::rebuildTabs);

    rebuildTabs();

    using Tabs = std::initializer_list<AbstractSearchWidget*>;
    for (auto tab: Tabs{ m_eventsTab, m_motionTab, m_bookmarksTab, m_analyticsTab })
        connect(tab, &AbstractSearchWidget::tileHovered, q, &EventPanel::tileHovered);

    connect(m_notificationsTab, &NotificationListWidget::tileHovered,
        q, &EventPanel::tileHovered);

    connect(m_notificationsTab, &NotificationListWidget::unreadCountChanged,
        q, &EventPanel::unreadCountChanged);

    q->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(q, &EventPanel::customContextMenuRequested,
        this, &EventPanel::Private::showContextMenu);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetAboutToBeChanged, this,
        [this]() { emit mediaWidgetAboutToBeChanged({}); });

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged,
        this, &Private::handleMediaWidgetChanged);
}

EventPanel::Private::~Private()
{
    // Required here for forward-declared scoped pointer destruction.
}

void EventPanel::Private::setTabCurrent(QWidget* tab, bool current)
{
    if (current)
    {
        m_tabs->setCurrentWidget(tab);
        NX_ASSERT(m_tabs->currentWidget() == tab);
    }
    else
    {
        if (m_previousTab && m_tabs->currentWidget() == tab)
            m_tabs->setCurrentWidget(m_previousTab);
    }
}

void EventPanel::Private::setTimeContentDisplayed(Qn::TimePeriodContent content, bool displayed)
{
    NX_ASSERT(content != Qn::RecordingContent);
    if (content == Qn::RecordingContent)
        return;

    if (displayed)
    {
        navigator()->setSelectedExtraContent(content);
    }
    else
    {
        if (navigator()->selectedExtraContent() == content)
            navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
    }
}

void EventPanel::Private::rebuildTabs()
{
    m_tabs->clear();
    m_previousTab = nullptr;
    m_lastTab = nullptr;

    const auto updateTab =
        [this](QWidget* tab, bool condition, const QIcon& icon, const QString& text)
        {
            if (condition)
            {
                const int index = m_tabs->count();
                m_tabs->insertTab(index, tab, icon, text);
                m_tabs->setTabToolTip(index, text);
            }
            else
            {
                tab->hide();
            }
        };

    updateTab(m_notificationsTab,
        true,
        qnSkin->icon(lit("events/tabs/notifications.png")),
        tr("Notifications", "Notifications tab title"));

    updateTab(m_motionTab,
        true,
        qnSkin->icon(lit("events/tabs/motion.png")),
        tr("Motion", "Motion tab title"));

    updateTab(m_bookmarksTab,
        accessController()->hasGlobalPermission(vms::api::GlobalPermission::viewBookmarks),
        qnSkin->icon(lit("events/tabs/bookmarks.png")),
        tr("Bookmarks", "Bookmarks tab title"));

    updateTab(m_eventsTab,
        accessController()->hasGlobalPermission(vms::api::GlobalPermission::viewLogs),
        qnSkin->icon(lit("events/tabs/events.png")),
        tr("Events", "Events tab title"));

    updateTab(m_analyticsTab,
        systemHasAnalytics(),
        qnSkin->icon(lit("events/tabs/analytics.png")),
        tr("Objects", "Analytics tab title"));
}

bool EventPanel::Private::systemHasAnalytics() const
{
    // TODO: #vkutin #GDM Implement when it becomes possible.
    return true;
}

void EventPanel::Private::updateUnreadCounter(int count, QnNotificationLevel::Value importance)
{
    m_counterLabel->setVisible(count > 0);
    if (count == 0)
        return;

    const auto text = (count > 99) ? lit("99+") : QString::number(count);
    const auto color = QnNotificationLevel::notificationTextColor(importance);

    m_counterLabel->setText(text);
    m_counterLabel->setBackgroundColor(color);

    const auto width = m_counterLabel->minimumSizeHint().width();

    static constexpr int kTopMargin = 6;
    static constexpr int kRightBoundaryPosition = 32;
    m_counterLabel->setGeometry(kRightBoundaryPosition - width, kTopMargin,
        width, m_counterLabel->minimumHeight());
}

void EventPanel::Private::showContextMenu(const QPoint& pos)
{
    QMenu contextMenu;
    contextMenu.addAction(action(ui::action::OpenBusinessLogAction));
    contextMenu.addAction(action(ui::action::BusinessEventsAction));
    contextMenu.addAction(action(ui::action::PreferencesNotificationTabAction));
    contextMenu.addSeparator();
    contextMenu.addAction(action(ui::action::PinNotificationsAction));
    contextMenu.exec(QnHiDpiWorkarounds::safeMapToGlobal(q, pos));
}

void EventPanel::Private::handleMediaWidgetChanged()
{
    const auto signalGuard = nx::utils::makeScopeGuard(
        [this]() { emit mediaWidgetChanged({}); });

    m_mediaWidget = qobject_cast<QnMediaResourceWidget*>(navigator()->currentWidget());
    if (!m_mediaWidget)
        return;

    if (!navigator()->currentResource().dynamicCast<QnVirtualCameraResource>())
        m_mediaWidget = nullptr;
}

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::MotionSearchSynchronizer

EventPanel::Private::MotionSearchSynchronizer::MotionSearchSynchronizer(EventPanel::Private* main):
    m_main(main)
{
    connect(m_main, &Private::mediaWidgetAboutToBeChanged, this,
        [this]()
        {
            if (!m_main->m_mediaWidget)
                return;

            m_main->m_mediaWidget->disconnect(this);
            m_main->m_mediaWidget->setMotionSearchModeEnabled(false);
        });

    connect(m_main, &Private::mediaWidgetChanged, this,
        [this]()
        {
            updateAreaSelection();
            if (!m_main->m_mediaWidget)
                return;

            m_main->m_mediaWidget->setMotionSearchModeEnabled(isMotionTab());

            connect(m_main->m_mediaWidget, &QnMediaResourceWidget::motionSearchModeEnabled, this,
                [this](bool enabled) { m_main->setTabCurrent(m_main->m_motionTab, enabled); });

            connect(m_main->m_mediaWidget, &QnMediaResourceWidget::motionSelectionChanged,
                this, &MotionSearchSynchronizer::updateAreaSelection);
        });

    connect(m_main->m_tabs, &QTabWidget::currentChanged, this,
        [this]()
        {
            updateAreaSelection();
            m_main->setTimeContentDisplayed(Qn::MotionContent, isMotionTab());

            if (m_main->m_mediaWidget)
                m_main->m_mediaWidget->setMotionSearchModeEnabled(isMotionTab());
        });

    connect(m_main->m_motionTab, &MotionSearchWidget::filterRegionsChanged, this,
        [this](const QList<QRegion>& value)
        {
            if (m_main->m_mediaWidget && m_main->m_mediaWidget->isMotionSearchModeEnabled())
                m_main->m_mediaWidget->setMotionSelection(value);
        });
}

bool EventPanel::Private::MotionSearchSynchronizer::isMotionTab() const
{
    return m_main->m_tabs->currentWidget() == m_main->m_motionTab;
}

void EventPanel::Private::MotionSearchSynchronizer::updateAreaSelection()
{
    if (m_main->m_mediaWidget && isMotionTab())
        m_main->m_motionTab->setFilterRegions(m_main->m_mediaWidget->motionSelection());
}

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::BookmarkSearchSynchronizer

EventPanel::Private::BookmarkSearchSynchronizer::BookmarkSearchSynchronizer(
    EventPanel::Private* main)
{
    connect(main->context()->action(ui::action::BookmarksModeAction), &QAction::toggled, this,
        [main](bool on) { main->setTabCurrent(main->m_bookmarksTab, on); });

    connect(main->m_tabs, &QTabWidget::currentChanged, this,
        [main]()
        {
            main->context()->action(ui::action::BookmarksModeAction)->setChecked(
                main->m_tabs->currentWidget() == main->m_bookmarksTab);
        });

    const auto updateTimelineBookmarks =
        [main]()
        {
            auto watcher = main->context()->instance<QnTimelineBookmarksWatcher>();
            if (!watcher)
                return;

            const auto currentCamera = main->q->navigator()->currentResource()
                .dynamicCast<QnVirtualCameraResource>();

            const bool relevant = currentCamera
                && main->m_bookmarksTab->cameras().contains(currentCamera);

            watcher->setTextFilter(relevant ? main->m_bookmarksTab->textFilter() : QString());
        };

    connect(main->m_bookmarksTab, &BookmarkSearchWidget::cameraSetChanged,
        this, updateTimelineBookmarks);

    connect(main->q->navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, updateTimelineBookmarks);
}

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::AnalyticsSearchSynchronizer

EventPanel::Private::AnalyticsSearchSynchronizer::AnalyticsSearchSynchronizer(
    EventPanel::Private* main)
    :
    m_main(main)
{
    connect(main, &Private::mediaWidgetAboutToBeChanged, this,
        [this]()
        {
            if (!m_main->m_mediaWidget)
                return;

            m_main->m_mediaWidget->disconnect(this);
            m_main->m_mediaWidget->unsetAreaSelectionType(QnMediaResourceWidget::AreaType::analytics);
        });

    connect(main, &Private::mediaWidgetChanged, this,
        [this]()
        {
            if (!m_main->m_mediaWidget)
                return;

            updateAreaSelection();

            connect(m_main->m_mediaWidget, &QnMediaResourceWidget::analyticsFilterRectChanged, this,
                [this]()
                {
                    updateAreaSelection();

                    // Stop selection after selecting once.
                    m_main->m_mediaWidget->setAreaSelectionEnabled(
                        QnMediaResourceWidget::AreaType::analytics, false);
                });
        });

    connect(m_main->m_tabs, &QTabWidget::currentChanged, this,
        [this]()
        {
            updateAreaSelection();
            updateTimelineDisplay();
        });

    connect(main->m_analyticsTab, &AnalyticsSearchWidget::areaSelectionEnabledChanged,
        this, &AnalyticsSearchSynchronizer::updateAreaSelection);

    connect(main->m_analyticsTab, &AnalyticsSearchWidget::areaSelectionRequested, this,
        [this]()
        {
            if (m_main->m_mediaWidget && isAnalyticsTab())
            {
                m_main->m_mediaWidget->setAreaSelectionEnabled(
                    QnMediaResourceWidget::AreaType::analytics, true);
            }
        });

    connect(m_main->m_analyticsTab, &AnalyticsSearchWidget::cameraSetChanged,
        this, &AnalyticsSearchSynchronizer::updateTimelineDisplay);

    connect(m_main->m_analyticsTab, &AnalyticsSearchWidget::textFilterChanged,
        this, &AnalyticsSearchSynchronizer::updateTimelineDisplay);

    connect(m_main->m_analyticsTab, &AnalyticsSearchWidget::filterRectChanged, this,
        [this](const QRectF& value)
        {
            updateTimelineDisplay();
            if (value.isNull() && m_main->m_mediaWidget
                && m_main->m_analyticsTab->areaSelectionEnabled())
            {
                m_main->m_mediaWidget->setAnalyticsFilterRect({});
            }
        });

    connect(m_main->q->navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &AnalyticsSearchSynchronizer::updateTimelineDisplay);
}

bool EventPanel::Private::AnalyticsSearchSynchronizer::isAnalyticsTab() const
{
    return m_main->m_tabs->currentWidget() == m_main->m_analyticsTab;
}

void EventPanel::Private::AnalyticsSearchSynchronizer::updateAreaSelection()
{
    if (!m_main->m_mediaWidget || !isAnalyticsTab())
        return;

    m_main->m_analyticsTab->setFilterRect(m_main->m_analyticsTab->areaSelectionEnabled()
        ? m_main->m_mediaWidget->analyticsFilterRect()
        : QRectF());

    m_main->m_mediaWidget->setAreaSelectionType(m_main->m_analyticsTab->areaSelectionEnabled()
        ? QnMediaResourceWidget::AreaType::analytics
        : QnMediaResourceWidget::AreaType::none);
}

void EventPanel::Private::AnalyticsSearchSynchronizer::updateTimelineDisplay()
{
    if (!isAnalyticsTab())
    {
        m_main->setTimeContentDisplayed(Qn::AnalyticsContent, false);
        return;
    }

    const auto currentCamera = m_main->q->navigator()->currentResource()
        .dynamicCast<QnVirtualCameraResource>();

    const bool relevant = currentCamera && m_main->m_analyticsTab->cameras().contains(currentCamera);
    if (!relevant)
    {
        m_main->q->navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
        return;
    }

    analytics::storage::Filter filter;
    filter.deviceIds = {currentCamera->getId()};
    filter.boundingBox = m_main->m_analyticsTab->filterRect();
    filter.freeText = m_main->m_analyticsTab->textFilter();
    m_main->q->navigator()->setAnalyticsFilter(filter);
    m_main->q->navigator()->setSelectedExtraContent(Qn::AnalyticsContent);
}

} // namespace nx::vms::client::desktop
