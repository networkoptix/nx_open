#include "event_panel_p.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/resource.h>
#include <ui/common/notification_levels.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/vms/client/desktop/common/widgets/animated_tab_widget.h>
#include <nx/vms/client/desktop/common/widgets/compact_tab_bar.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/event_search/synchronizers/analytics_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/synchronizers/bookmark_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/synchronizers/motion_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/event_search/widgets/simple_motion_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/bookmark_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/event_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/analytics_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/notification_list_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/notification_counter_label.h>
#include <nx/utils/range_adapters.h>
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
    m_motionTab(new SimpleMotionSearchWidget(context(), m_tabs)),
    m_bookmarksTab(new BookmarkSearchWidget(context(), m_tabs)),
    m_eventsTab(new EventSearchWidget(context(), m_tabs)),
    m_analyticsTab(new AnalyticsSearchWidget(context(), m_tabs)),
    m_synchronizers({
        {m_motionTab, new MotionSearchSynchronizer(context(), m_motionTab, this)},
        {m_bookmarksTab, new BookmarkSearchSynchronizer(context(), m_bookmarksTab, this)},
        {m_analyticsTab, new AnalyticsSearchSynchronizer(context(), m_analyticsTab, this)}})
{
    auto layout = new QVBoxLayout(q);
    layout->setContentsMargins(QMargins());
    layout->addWidget(m_tabs);

    // Initially all tab widgets must be hidden.
    m_notificationsTab->hide();
    m_motionTab->hide();
    m_bookmarksTab->hide();
    m_eventsTab->hide();
    m_analyticsTab->hide();

    static constexpr int kTabBarShift = 10;
    m_tabs->setProperty(style::Properties::kTabBarIndent, kTabBarShift);
    setTabShape(m_tabs->tabBar(), style::TabShape::Compact);

    for (auto [tab, synchronizer]: nx::utils::keyValueRange(m_synchronizers))
    {
        connect(synchronizer, &AbstractSearchSynchronizer::activeChanged, this,
            [this, tab = tab](bool isActive) { setTabCurrent(tab, isActive); });
    }

    connect(m_tabs, &QTabWidget::currentChanged, this,
        [this]()
        {
            m_previousTab = m_lastTab;
            m_lastTab = m_tabs->currentWidget();

            for (auto [tab, synchronizer]: nx::utils::keyValueRange(m_synchronizers))
                synchronizer->setActive(m_tabs->currentWidget() == tab);

            NX_VERBOSE(this->q, "Tab changed; previous: %1, current: %2", m_previousTab, m_lastTab);
        });

    q->setAutoFillBackground(false);
    q->setAttribute(Qt::WA_TranslucentBackground);

    m_counterLabel->hide();

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

    using Tabs = std::initializer_list<AbstractSearchWidget*>;
    for (auto tab: Tabs{m_eventsTab, m_motionTab, m_bookmarksTab, m_analyticsTab})
    {
        connect(tab, &AbstractSearchWidget::tileHovered, q, &EventPanel::tileHovered);
        connect(tab, &AbstractSearchWidget::isAllowedChanged, this, &Private::rebuildTabs);
    }

    rebuildTabs();

    connect(m_notificationsTab, &NotificationListWidget::tileHovered,
        q, &EventPanel::tileHovered);

    connect(m_notificationsTab, &NotificationListWidget::unreadCountChanged,
        q, &EventPanel::unreadCountChanged);

    q->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(q, &EventPanel::customContextMenuRequested,
        this, &EventPanel::Private::showContextMenu);
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

void EventPanel::Private::rebuildTabs()
{
    int currentIndex = 0;

    const auto updateTab =
        [this, &currentIndex](QWidget* tab, bool condition, const QIcon& icon, const QString& text)
        {
            const int oldIndex = m_tabs->indexOf(tab);
            NX_ASSERT(oldIndex < 0 || oldIndex == currentIndex);

            const bool tabVisible = oldIndex >= 0;
            if (condition)
            {
                if (!tabVisible)
                {
                    // Add tab.
                    m_tabs->insertTab(currentIndex, tab, icon, text);
                    m_tabs->setTabToolTip(currentIndex, text);
                }

                ++currentIndex;
            }
            else if (tabVisible)
            {
                // Remove tab.
                if (m_previousTab == tab || m_tabs->indexOf(m_previousTab) < 0)
                    m_previousTab = m_notificationsTab;
                setTabCurrent(tab, false);
                m_tabs->removeTab(currentIndex);
                tab->hide();
            }
        };

    const auto resource = navigator()->currentResource();

    updateTab(m_notificationsTab,
        true,
        qnSkin->icon(lit("events/tabs/notifications.png")),
        tr("Notifications", "Notifications tab title"));

    updateTab(m_motionTab,
        m_motionTab->isAllowed(),
        qnSkin->icon(lit("events/tabs/motion.png")),
        tr("Motion", "Motion tab title"));

    updateTab(m_bookmarksTab,
        m_bookmarksTab->isAllowed(),
        qnSkin->icon(lit("events/tabs/bookmarks.png")),
        tr("Bookmarks", "Bookmarks tab title"));

    updateTab(m_eventsTab,
        m_eventsTab->isAllowed(),
        qnSkin->icon(lit("events/tabs/events.png")),
        tr("Events", "Events tab title"));

    updateTab(m_analyticsTab,
        m_analyticsTab->isAllowed(),
        qnSkin->icon(lit("events/tabs/analytics.png")),
        tr("Objects", "Analytics tab title"));
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
    const auto actions = {
        ui::action::OpenBusinessLogAction,
        ui::action::BusinessEventsAction,
        ui::action::PreferencesNotificationTabAction};

    for (const auto actionId: actions)
    {
        if (menu()->canTrigger(actionId))
            contextMenu.addAction(action(actionId));
    }

    contextMenu.addSeparator();
    contextMenu.addAction(action(ui::action::PinNotificationsAction));

    contextMenu.exec(QnHiDpiWorkarounds::safeMapToGlobal(q, pos));
}

} // namespace nx::vms::client::desktop
