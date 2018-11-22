#pragma once

#include "../event_panel.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

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

class MotionSearchWidget;
class BookmarkSearchWidget;
class EventSearchWidget;
class AnalyticsSearchWidget;

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

    void setTabCurrent(QWidget* tab, bool current);
    void setTimeContentDisplayed(Qn::TimePeriodContent content, bool displayed);

signals:
    void mediaWidgetAboutToBeChanged(QPrivateSignal);
    void mediaWidgetChanged(QPrivateSignal);

private:
    void rebuildTabs();
    bool systemHasAnalytics() const;
    void updateUnreadCounter(int count, QnNotificationLevel::Value importance);
    void showContextMenu(const QPoint& pos);
    void handleMediaWidgetChanged();

private:
    EventPanel* const q;
    QTabWidget* const m_tabs;

    NotificationListWidget* const m_notificationsTab;
    NotificationCounterLabel* const m_counterLabel;

    MotionSearchWidget* const m_motionTab;
    BookmarkSearchWidget* const m_bookmarksTab;
    EventSearchWidget* const m_eventsTab;
    AnalyticsSearchWidget* const m_analyticsTab;

    QWidget* m_previousTab = nullptr;
    QWidget* m_lastTab = nullptr;

    QPointer<QnMediaResourceWidget> m_mediaWidget;

    class MotionSearchSynchronizer;
    const QScopedPointer<MotionSearchSynchronizer> m_motionSearchSynchronizer;

    class BookmarkSearchSynchronizer;
    const QScopedPointer<BookmarkSearchSynchronizer> m_bookmarkSearchSynchronizer;

    class AnalyticsSearchSynchronizer;
    const QScopedPointer<AnalyticsSearchSynchronizer> m_analyticsSearchSynchronizer;
};

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::MotionSearchSynchronizer

class EventPanel::Private::MotionSearchSynchronizer: public QObject
{
    EventPanel::Private* const m_main;

public:
    MotionSearchSynchronizer(EventPanel::Private* main);

private:
    bool isMotionTab() const;
    void updateAreaSelection();
};

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::AnalyticsSearchSynchronizer

class EventPanel::Private::AnalyticsSearchSynchronizer: public QObject
{
    EventPanel::Private* const m_main;

public:
    AnalyticsSearchSynchronizer(EventPanel::Private* main);

private:
    bool isAnalyticsTab() const;
    void updateAreaSelection();
    void updateTimelineDisplay();
};

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::BookmarkSearchSynchronizer

class EventPanel::Private::BookmarkSearchSynchronizer: public QObject
{
public:
    BookmarkSearchSynchronizer(EventPanel::Private* main);
};

} // namespace nx::vms::client::desktop
