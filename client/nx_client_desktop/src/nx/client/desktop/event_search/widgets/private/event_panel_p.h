#pragma once

#include "../event_panel.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

#include <nx/utils/disconnect_helper.h>

class QTabWidget;
class QStackedWidget;
class QnMediaResourceWidget;
class QAbstractItemModel;

namespace QnNotificationLevel { enum class Value; }

namespace nx::vms::event { class StringsHelper; }

namespace nx::client::desktop {

class NotificationListWidget;
class NotificationCounterLabel;

class MotionSearchWidget;
class BookmarkSearchWidget;
class EventSearchWidget;
class AnalyticsSearchWidget;

class EventPanel::Private: public QObject
{
    Q_OBJECT

public:
    Private(EventPanel* q);
    virtual ~Private() override;

private:
    void rebuildTabs();
    bool systemHasAnalytics() const;
    void updateUnreadCounter(int count, QnNotificationLevel::Value importance);
    void showContextMenu(const QPoint& pos);

    void currentWorkbenchWidgetChanged(Qn::ItemRole role);
    void setupTabsSyncWithNavigator();

    void at_motionSearchToggled(bool on);
    void at_bookmarksToggled(bool on);
    void at_specialModeToggled(bool on, QWidget* correspondingTab);

private:
    EventPanel* const q;
    QTabWidget* const m_tabs;

    NotificationListWidget* const m_notificationsTab;
    NotificationCounterLabel* const m_counterLabel;

    MotionSearchWidget* const m_motionTab;
    BookmarkSearchWidget* const m_bookmarksTab;
    EventSearchWidget* const m_eventsTab;
    AnalyticsSearchWidget* const m_analyticsTab;

    QPointer<QnMediaResourceWidget> m_currentMediaWidget;
    QScopedPointer<QnDisconnectHelper> m_mediaWidgetConnections;

    int m_previousTabIndex = 0;
    int m_lastTabIndex = 0;
};

} // namespace nx::client::desktop
