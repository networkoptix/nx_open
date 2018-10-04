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
#include <ui/workbench/workbench_display.h>
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
            if (m_tabs->currentIndex() != index || !m_tabs->currentWidget())
                return;

            // TODO: #vkutin Replace with some setLive(true).
            if (auto ribbon = m_tabs->currentWidget()->findChild<EventRibbon*>())
                ribbon->scrollBar()->setValue(0);
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

    connect(q->context()->display(), &QnWorkbenchDisplay::widgetChanged,
        this, &Private::currentWorkbenchWidgetChanged, Qt::QueuedConnection);

    setupTabsSyncWithNavigator();
}

EventPanel::Private::~Private()
{
}

void EventPanel::Private::rebuildTabs()
{
    m_tabs->clear();

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

void EventPanel::Private::currentWorkbenchWidgetChanged(Qn::ItemRole role)
{
    // TODO: FIXME: #vkutin Rewrite this code properly.

    if (role != Qn::CentralRole)
        return;

    m_mediaWidgetConnections.reset();

    m_currentMediaWidget = qobject_cast<QnMediaResourceWidget*>(
        this->q->context()->display()->widget(Qn::CentralRole));

    const auto camera = m_currentMediaWidget
        ? m_currentMediaWidget->resource().dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr();

    if (!camera)
        return;

    m_mediaWidgetConnections.reset(new QnDisconnectHelper());

    *m_mediaWidgetConnections << connect(m_currentMediaWidget.data(),
        &QnMediaResourceWidget::motionSearchModeEnabled, this, &Private::at_motionSearchToggled);

    at_specialModeToggled(m_currentMediaWidget->isMotionSearchModeEnabled(), m_motionTab);
}

void EventPanel::Private::setupTabsSyncWithNavigator()
{
    // TODO: FIXME: #vkutin Adapt for multi-camera mode.

    connect(m_tabs, &QTabWidget::currentChanged, this,
        [this](int index)
        {
            q->context()->action(ui::action::BookmarksModeAction)->setChecked(
                m_tabs->currentWidget() == m_bookmarksTab);

            if (m_currentMediaWidget)
            {
                QnResourceWidgetList widgets({m_currentMediaWidget});
                if (m_tabs->currentWidget() == m_motionTab)
                    q->menu()->trigger(ui::action::StartSmartSearchAction, widgets);
                else
                    q->menu()->trigger(ui::action::StopSmartSearchAction, widgets);
            }

            auto extraContent = Qn::RecordingContent;
            if (m_tabs->currentWidget() == m_motionTab)
                extraContent = Qn::MotionContent;
            else if (m_tabs->currentWidget() == m_analyticsTab)
                extraContent = Qn::AnalyticsContent;

            q->navigator()->setSelectedExtraContent(extraContent);

            m_previousTabIndex = m_lastTabIndex;
            m_lastTabIndex = index;
        });

    connect(q->context()->action(ui::action::BookmarksModeAction), &QAction::toggled,
        this, &Private::at_bookmarksToggled);
}

void EventPanel::Private::at_motionSearchToggled(bool on)
{
    at_specialModeToggled(on, m_motionTab);
}

void EventPanel::Private::at_bookmarksToggled(bool on)
{
    at_specialModeToggled(on, m_bookmarksTab);
}

void EventPanel::Private::at_specialModeToggled(bool on, QWidget* correspondingTab)
{
    const auto index = m_tabs->indexOf(correspondingTab);
    if (index < 0)
        return;

    if (on)
    {
        m_tabs->setCurrentIndex(index);
    }
    else
    {
        if (m_tabs->currentWidget() == correspondingTab)
            m_tabs->setCurrentIndex(m_previousTabIndex);
    }
}

} // namespace nx::client::desktop
