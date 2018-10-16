#pragma once

#include "../event_panel.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

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
    void setupTabSyncWithNavigator();
    void setTabEnforced(QWidget* tab, bool enforced);
    void setCurrentWidgetMotionSearch(bool value);
    void updateCurrentWidgetMotionSearch();
    bool singleCameraMotionSearch() const;

    void handleCurrentMediaWidgetChanged();
    void handleWidgetMotionSearchChanged(bool on);

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

    QWidget* m_previousTab = nullptr;
    QWidget* m_lastTab = nullptr;
};

} // namespace nx::client::desktop
