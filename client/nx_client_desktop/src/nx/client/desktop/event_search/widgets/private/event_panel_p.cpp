#include "event_panel_p.h"

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

namespace nx::client::desktop {

EventPanel::Private::Private(EventPanel* q):
    QObject(q),
    q(q),
    m_tabs(new AnimatedTabWidget(new CompactTabBar(), q)),
    m_notificationsTab(new NotificationListWidget(m_tabs)),
    m_counterLabel(new NotificationCounterLabel(m_tabs->tabBar())),
    m_motionTab(new MotionSearchWidget(q->context(), m_tabs)),
    m_bookmarksTab(new BookmarkSearchWidget(q->context(), m_tabs)),
    m_eventsTab(new EventSearchWidget(q->context(), m_tabs)),
    m_analyticsTab(new AnalyticsSearchWidget(q->context(), m_tabs))
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

    setupTabSyncWithNavigator();
}

EventPanel::Private::~Private()
{
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

void EventPanel::Private::setupTabSyncWithNavigator()
{
    connect(m_tabs, &QTabWidget::currentChanged, this,
        [this](int index)
        {
            q->context()->action(ui::action::BookmarksModeAction)->setChecked(
                m_tabs->currentWidget() == m_bookmarksTab);

            updateCurrentWidgetMotionSearch();

            auto extraContent = Qn::RecordingContent;
            if (m_tabs->currentWidget() == m_motionTab)
                extraContent = Qn::MotionContent;
            else if (m_tabs->currentWidget() == m_analyticsTab)
                extraContent = Qn::AnalyticsContent;

            q->navigator()->setSelectedExtraContent(extraContent);

            m_previousTab = m_lastTab;
            m_lastTab = m_tabs->currentWidget();
        });

    connect(m_motionTab, &AbstractSearchWidget::cameraSetChanged,
        this, &Private::updateCurrentWidgetMotionSearch);

    connect(q->navigator(), &QnWorkbenchNavigator::currentWidgetChanged,
        this, &Private::handleCurrentMediaWidgetChanged, Qt::QueuedConnection);

    connect(q->context()->action(ui::action::BookmarksModeAction), &QAction::toggled, this,
        [this](bool on) { setTabEnforced(m_bookmarksTab, on); });
}

void EventPanel::Private::handleCurrentMediaWidgetChanged()
{
    // Turn Motion Search off for previously selected media widget.
    if (m_currentMediaWidget)
    {
        // Order of the next two lines of code is important.
        m_currentMediaWidget->disconnect(this);
        setCurrentWidgetMotionSearch(false);
    }

    m_currentMediaWidget = qobject_cast<QnMediaResourceWidget*>(q->navigator()->currentWidget());
    if (!m_currentMediaWidget)
        return;

    if (!q->navigator()->currentResource().dynamicCast<QnVirtualCameraResource>())
    {
        m_currentMediaWidget = nullptr;
        return;
    }

    connect(m_currentMediaWidget.data(), &QnMediaResourceWidget::motionSearchModeEnabled,
        this, &Private::handleWidgetMotionSearchChanged);

    // If Right Panel is in current camera motion search mode,
    // enable Motion Search for newly selected media widget.
    if (singleCameraMotionSearch())
        updateCurrentWidgetMotionSearch();
    // Otherwise, if Motion Search is activated for newly selected media widget,
    // switch Right Panel into current camera motion search mode.
    else if (m_currentMediaWidget->isMotionSearchModeEnabled())
        handleWidgetMotionSearchChanged(true);
}

void EventPanel::Private::handleWidgetMotionSearchChanged(bool on)
{
    if (on && m_tabs->currentWidget() == m_motionTab)
        m_previousTab = m_motionTab;

    m_motionTab->setCurrentMotionSearchEnabled(on);
    setTabEnforced(m_motionTab, on);
}

void EventPanel::Private::setTabEnforced(QWidget* tab, bool enforced)
{
    const auto index = m_tabs->indexOf(tab);
    if (index < 0)
        return;

    if (enforced)
    {
        m_tabs->setCurrentIndex(index);
    }
    else
    {
        if (m_tabs->currentWidget() == tab)
            m_tabs->setCurrentWidget(m_previousTab);
    }
}

void EventPanel::Private::setCurrentWidgetMotionSearch(bool value)
{
    if (m_currentMediaWidget && m_currentMediaWidget->isMotionSearchModeEnabled() != value)
        m_currentMediaWidget->setMotionSearchModeEnabled(value);
}

void EventPanel::Private::updateCurrentWidgetMotionSearch()
{
    setCurrentWidgetMotionSearch(singleCameraMotionSearch());
}

bool EventPanel::Private::singleCameraMotionSearch() const
{
    return m_tabs->currentWidget() == m_motionTab
        && m_motionTab->selectedCameras() == AbstractSearchWidget::Cameras::current;
}

} // namespace nx::client::desktop
