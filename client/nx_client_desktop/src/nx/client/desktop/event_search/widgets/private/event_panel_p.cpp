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
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/client/desktop/common/widgets/animated_tab_widget.h>
#include <nx/client/desktop/common/widgets/compact_tab_bar.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/client/desktop/event_search/widgets/motion_search_widget.h>
#include <nx/client/desktop/event_search/widgets/bookmark_search_widget.h>
#include <nx/client/desktop/event_search/widgets/event_search_widget.h>
#include <nx/client/desktop/event_search/widgets/analytics_search_widget.h>
#include <nx/client/desktop/event_search/widgets/notification_list_widget.h>
#include <nx/client/desktop/event_search/widgets/notification_counter_label.h>
#include <nx/utils/scope_guard.h>

namespace nx::client::desktop {

// ------------------------------------------------------------------------------------------------
// EventPanel::Private

EventPanel::Private::Private(EventPanel* q):
    QObject(q),
    q(q),
    m_tabs(new AnimatedTabWidget(new CompactTabBar(), q)),
    m_notificationsTab(new NotificationListWidget(m_tabs)),
    m_counterLabel(new NotificationCounterLabel(m_tabs->tabBar())),
    m_motionTab(new MotionSearchWidget(q->context(), m_tabs)),
    m_bookmarksTab(new BookmarkSearchWidget(q->context(), m_tabs)),
    m_eventsTab(new EventSearchWidget(q->context(), m_tabs)),
    m_analyticsTab(new AnalyticsSearchWidget(q->context(), m_tabs)),
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

    connect(q->accessController(), &QnWorkbenchAccessController::globalPermissionsChanged,
        this, &Private::rebuildTabs);

    rebuildTabs();

    using Tabs = std::initializer_list<AbstractSearchWidget*>;
    for (auto tab: Tabs{m_eventsTab, m_motionTab, m_bookmarksTab, m_analyticsTab})
        connect(tab, &AbstractSearchWidget::tileHovered, q, &EventPanel::tileHovered);

    connect(m_notificationsTab, &NotificationListWidget::tileHovered,
        q, &EventPanel::tileHovered);

    connect(m_notificationsTab, &NotificationListWidget::unreadCountChanged,
        q, &EventPanel::unreadCountChanged);

    q->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(q, &EventPanel::customContextMenuRequested,
        this, &EventPanel::Private::showContextMenu);

    connect(q->navigator(), &QnWorkbenchNavigator::currentWidgetAboutToBeChanged, this,
        [this]() { emit currentMediaWidgetAboutToBeChanged({}); });

    connect(q->navigator(), &QnWorkbenchNavigator::currentWidgetChanged,
        this, &Private::handleCurrentMediaWidgetChanged);
}

EventPanel::Private::~Private()
{
    // Required here for forward-declared scoped pointer destruction.
}

void EventPanel::Private::rebuildTabs()
{
    m_tabs->clear();
    m_motionSearchSynchronizer->reset();
    m_bookmarkSearchSynchronizer->reset();
    m_analyticsSearchSynchronizer->reset();

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
        q->accessController()->hasGlobalPermission(vms::api::GlobalPermission::viewBookmarks),
        qnSkin->icon(lit("events/tabs/bookmarks.png")),
        tr("Bookmarks", "Bookmarks tab title"));

    updateTab(m_eventsTab,
        q->accessController()->hasGlobalPermission(vms::api::GlobalPermission::viewLogs),
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
    contextMenu.addAction(q->action(ui::action::OpenBusinessLogAction));
    contextMenu.addAction(q->action(ui::action::BusinessEventsAction));
    contextMenu.addAction(q->action(ui::action::PreferencesNotificationTabAction));
    contextMenu.addSeparator();
    contextMenu.addAction(q->action(ui::action::PinNotificationsAction));
    contextMenu.exec(QnHiDpiWorkarounds::safeMapToGlobal(q, pos));
}

void EventPanel::Private::handleCurrentMediaWidgetChanged()
{
    const auto signalGuard = utils::makeScopeGuard(
        [this]() { emit currentMediaWidgetChanged({}); });

    m_currentMediaWidget = qobject_cast<QnMediaResourceWidget*>(q->navigator()->currentWidget());
    if (!m_currentMediaWidget)
        return;

    if (!q->navigator()->currentResource().dynamicCast<QnVirtualCameraResource>())
        m_currentMediaWidget = nullptr;
}

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::MotionSearchSynchronizer

EventPanel::Private::MotionSearchSynchronizer::MotionSearchSynchronizer(EventPanel::Private* main):
    m_main(main)
{
    connect(m_main, &Private::currentMediaWidgetAboutToBeChanged,
        this, &MotionSearchSynchronizer::handleCurrentWidgetAboutToBeChanged);

    connect(m_main, &Private::currentMediaWidgetChanged,
        this, &MotionSearchSynchronizer::handleCurrentWidgetChanged);

    connect(m_main->m_motionTab, &AbstractSearchWidget::cameraSetChanged,
        this, &MotionSearchSynchronizer::updateState);

    connect(m_main->m_tabs, &QTabWidget::currentChanged,
        this, &MotionSearchSynchronizer::updateState);

    connect(m_main->m_motionTab, &AbstractSearchWidget::selectedAreaChanged, this,
        [this](bool wholeArea)
        {
            if (wholeArea && widget())
                widget()->clearMotionSelection();
        });
}

void EventPanel::Private::MotionSearchSynchronizer::reset()
{
    QScopedValueRollback updateGuard(m_updating, true);
    m_state = State::irrelevant;
    m_revertTab = nullptr;
    setCurrentWidgetMotionSearch(false);
}

void EventPanel::Private::MotionSearchSynchronizer::handleCurrentWidgetAboutToBeChanged()
{
    if (!widget())
        return;

    widget()->disconnect(this);
    setCurrentWidgetMotionSearch(false); //< Must be after signal disconnection.
}

void EventPanel::Private::MotionSearchSynchronizer::handleCurrentWidgetChanged()
{
    if (!widget())
        return;

    connect(widget(), &QnMediaResourceWidget::motionSearchModeEnabled,
        this, &MotionSearchSynchronizer::syncPanelWithWidget);

    connect(widget(), &QnMediaResourceWidget::motionSelectionChanged,
        this, &MotionSearchSynchronizer::handleMotionSelectionChanged);

    // If Right Panel is in current camera motion search mode,
    // enable Motion Search for newly selected media widget.
    if (m_state == State::syncedMotion)
        syncWidgetWithPanel();
    // Otherwise, if Motion Search is activated for newly selected media widget,
    // switch Right Panel into current camera motion search mode.
    else if (widget()->isMotionSearchModeEnabled())
        syncPanelWithWidget();
}

void EventPanel::Private::MotionSearchSynchronizer::handleMotionSelectionChanged()
{
    m_main->m_motionTab->setFilterRegions(widget() ? widget()->motionSelection() : QList<QRegion>());
}

void EventPanel::Private::MotionSearchSynchronizer::updateState()
{
    m_state = calculateState();
    syncWidgetWithPanel();
}

EventPanel::Private::MotionSearchSynchronizer::State
    EventPanel::Private::MotionSearchSynchronizer::calculateState() const
{
    if (m_main->m_tabs->currentWidget() != m_main->m_motionTab)
        return State::irrelevant;

    return m_main->m_motionTab->selectedCameras() == AbstractSearchWidget::Cameras::current
        ? State::syncedMotion
        : State::unsyncedMotion;
}

void EventPanel::Private::MotionSearchSynchronizer::setCurrentWidgetMotionSearch(bool value)
{
    if (widget() && widget()->isMotionSearchModeEnabled() != value)
        widget()->setMotionSearchModeEnabled(value);
}

void EventPanel::Private::MotionSearchSynchronizer::syncWidgetWithPanel()
{
    if (m_updating)
        return;

    QScopedValueRollback updateGuard(m_updating, true);
    setCurrentWidgetMotionSearch(m_state == State::syncedMotion);
    m_revertTab = nullptr;
}

void EventPanel::Private::MotionSearchSynchronizer::syncPanelWithWidget()
{
    if (m_updating)
        return;

    QScopedValueRollback updateGuard(m_updating, true);

    const bool on = widget()->isMotionSearchModeEnabled();
    const bool fromOtherTab = m_state == State::irrelevant;

    m_main->m_motionTab->setSingleCameraMode(on);
    if (on)
    {
        m_revertTab = fromOtherTab ? m_main->m_tabs->currentWidget() : nullptr;
        m_main->m_tabs->setCurrentWidget(m_main->m_motionTab);
        handleMotionSelectionChanged();
    }
    else
    {
        if (m_revertTab)
            m_main->m_tabs->setCurrentWidget(m_revertTab);
    }
}

QnMediaResourceWidget* EventPanel::Private::MotionSearchSynchronizer::widget() const
{
    return m_main->m_currentMediaWidget;
}

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::BookmarkSearchSynchronizer

EventPanel::Private::BookmarkSearchSynchronizer::BookmarkSearchSynchronizer(
    EventPanel::Private* main)
    :
    m_main(main)
{
    connect(main->m_tabs, &QTabWidget::currentChanged,
        this, &BookmarkSearchSynchronizer::handleCurrentTabChanged);

    connect(main->q->context()->action(ui::action::BookmarksModeAction), &QAction::toggled,
        this, &BookmarkSearchSynchronizer::setBookmarksTabActive);
}

void EventPanel::Private::BookmarkSearchSynchronizer::handleCurrentTabChanged()
{
    m_main->q->context()->action(ui::action::BookmarksModeAction)->setChecked(
        m_main->m_tabs->currentWidget() == m_main->m_bookmarksTab);

    m_revertTab = m_lastTab;
    m_lastTab = m_main->m_tabs->currentWidget();
}

void EventPanel::Private::BookmarkSearchSynchronizer::setBookmarksTabActive(bool value)
{
    if (value)
    {
        m_main->m_tabs->setCurrentWidget(m_main->m_bookmarksTab);
    }
    else
    {
        if (m_revertTab && m_main->m_tabs->currentWidget() == m_main->m_bookmarksTab)
            m_main->m_tabs->setCurrentWidget(m_revertTab);
    }
}

void EventPanel::Private::BookmarkSearchSynchronizer::reset()
{
    m_lastTab = nullptr;
    m_revertTab = nullptr;
    m_main->q->context()->action(ui::action::BookmarksModeAction)->setChecked(false);
}

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::AnalyticsSearchSynchronizer

EventPanel::Private::AnalyticsSearchSynchronizer::AnalyticsSearchSynchronizer(
    EventPanel::Private* main)
    :
    m_main(main)
{
    connect(m_main, &Private::currentMediaWidgetAboutToBeChanged,
        this, &AnalyticsSearchSynchronizer::handleCurrentWidgetAboutToBeChanged);

    connect(m_main, &Private::currentMediaWidgetChanged,
        this, &AnalyticsSearchSynchronizer::handleCurrentWidgetChanged);

    connect(m_main->m_analyticsTab, &AbstractSearchWidget::cameraSetChanged,
        this, &AnalyticsSearchSynchronizer::syncWidgetWithPanel);

    connect(m_main->m_tabs, &QTabWidget::currentChanged,
        this, &AnalyticsSearchSynchronizer::syncWidgetWithPanel);

    connect(m_main->m_analyticsTab, &AbstractSearchWidget::selectedAreaChanged, this,
        [this](bool wholeArea)
        {
            if (wholeArea && m_main->m_currentMediaWidget)
                m_main->m_currentMediaWidget->setAnalyticsSelection({});
        });
}

void EventPanel::Private::AnalyticsSearchSynchronizer::reset()
{
    setWidgetAnalyticsSelectionEnabled(false);
}

void EventPanel::Private::AnalyticsSearchSynchronizer::handleCurrentWidgetAboutToBeChanged()
{
    if (!m_main->m_currentMediaWidget)
        return;

    m_main->m_currentMediaWidget->disconnect(this);
    setWidgetAnalyticsSelectionEnabled(false);
}

void EventPanel::Private::AnalyticsSearchSynchronizer::handleCurrentWidgetChanged()
{
    if (!m_main->m_currentMediaWidget)
        return;

    connect(m_main->m_currentMediaWidget.data(), &QnMediaResourceWidget::analyticsSelectionChanged,
        this, &AnalyticsSearchSynchronizer::handleAnalyticsSelectionChanged);

    syncWidgetWithPanel();
    handleAnalyticsSelectionChanged();
}

void EventPanel::Private::AnalyticsSearchSynchronizer::handleAnalyticsSelectionChanged()
{
    if (!analyticsAreaSelection())
        return;

    m_main->m_analyticsTab->setFilterRect(m_main->m_currentMediaWidget
        ? m_main->m_currentMediaWidget->analyticsSelection()
        : QRectF());
}

bool EventPanel::Private::AnalyticsSearchSynchronizer::analyticsAreaSelection() const
{
    return m_main->m_tabs->currentWidget() == m_main->m_analyticsTab
        && m_main->m_analyticsTab->selectedCameras() == AbstractSearchWidget::Cameras::current;
}

void EventPanel::Private::AnalyticsSearchSynchronizer::setWidgetAnalyticsSelectionEnabled(
    bool value)
{
    if (m_main->m_currentMediaWidget)
        m_main->m_currentMediaWidget->setAnalyticsSelectionEnabled(value);
}

void EventPanel::Private::AnalyticsSearchSynchronizer::syncWidgetWithPanel()
{
    setWidgetAnalyticsSelectionEnabled(analyticsAreaSelection());
}

} // namespace nx::client::desktop
