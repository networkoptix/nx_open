#pragma once

#include "../event_panel.h"

#include <QtCore/QHash>

#include <common/common_globals.h>

class QTabWidget;
class QStackedWidget;
class QnMediaResourceWidget;
class QAbstractItemModel;
class QAction;

namespace QnNotificationLevel { enum class Value; }

namespace nx::vms::event { class StringsHelper; }

namespace nx::vms::client::desktop {

class NotificationListWidget;
class NotificationCounterLabel;

class AbstractSearchWidget;
class SimpleMotionSearchWidget;
class BookmarkSearchWidget;
class EventSearchWidget;
class AnalyticsSearchWidget;

class AbstractSearchSynchronizer;

// ------------------------------------------------------------------------------------------------
// EventPanel::Private

class EventPanel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    Private(EventPanel* q);
    virtual ~Private() override;

    Tab currentTab() const;
    bool setCurrentTab(Tab tab);

private:
    void rebuildTabs();
    void updateUnreadCounter(int count, QnNotificationLevel::Value importance);
    void showContextMenu(const QPoint& pos);
    void setTabCurrent(QWidget* tab, bool current);

private:
    EventPanel* const q;
    QTabWidget* const m_tabs;

    NotificationListWidget* const m_notificationsTab;
    NotificationCounterLabel* const m_counterLabel;

    SimpleMotionSearchWidget* const m_motionTab;
    BookmarkSearchWidget* const m_bookmarksTab;
    EventSearchWidget* const m_eventsTab;
    AnalyticsSearchWidget* const m_analyticsTab;

    const QHash<AbstractSearchWidget*, AbstractSearchSynchronizer*> m_synchronizers;
    const QHash<QWidget*, Tab> m_tabIds;

    QWidget* m_previousTab = nullptr;
    QWidget* m_lastTab = nullptr;
};

} // namespace nx::vms::client::desktop
