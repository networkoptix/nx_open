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

// ------------------------------------------------------------------------------------------------
// EventPanel::Private

class EventPanel::Private: public QObject
{
    Q_OBJECT

public:
    Private(EventPanel* q);
    virtual ~Private() override;

signals:
    void currentMediaWidgetAboutToBeChanged(QPrivateSignal);
    void currentMediaWidgetChanged(QPrivateSignal);

private:
    void rebuildTabs();
    bool systemHasAnalytics() const;
    void updateUnreadCounter(int count, QnNotificationLevel::Value importance);
    void showContextMenu(const QPoint& pos);
    void handleCurrentMediaWidgetChanged();

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
public:
    MotionSearchSynchronizer(EventPanel::Private* main);
    void reset();

private:
    void handleCurrentWidgetAboutToBeChanged();
    void handleCurrentWidgetChanged();
    void handleMotionSelectionChanged();

    void updateState();
    void syncWidgetWithPanel();
    void syncPanelWithWidget();

    enum class State
    {
        irrelevant, //< Other than Motion tab is selected.
        unsyncedMotion, //< Motion tab and other than current camera mode are selected.
        syncedMotion //< Motion tab and current camera mode are selected.
    };

    State calculateState() const;

    void setCurrentWidgetMotionSearch(bool value);
    QnMediaResourceWidget* widget() const;

private:
    EventPanel::Private* const m_main;
    State m_state = State::irrelevant;
    bool m_updating = false;
    QWidget* m_revertTab = nullptr;
};

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::AnalyticsSearchSynchronizer

class EventPanel::Private::AnalyticsSearchSynchronizer: public QObject
{
public:
    AnalyticsSearchSynchronizer(EventPanel::Private* main);
    void reset();

private:
    void handleCurrentWidgetAboutToBeChanged();
    void handleCurrentWidgetChanged();
    void handleAnalyticsSelectionChanged();

    void syncWidgetWithPanel();
    bool analyticsAreaSelection() const; //< Whether analytics area selection mode is required.
    void setWidgetAnalyticsSelectionEnabled(bool value);

private:
    EventPanel::Private* const m_main;
};

// ------------------------------------------------------------------------------------------------
// EventPanel::Private::BookmarkSearchSynchronizer

class EventPanel::Private::BookmarkSearchSynchronizer: public QObject
{
public:
    BookmarkSearchSynchronizer(EventPanel::Private* main);
    void reset();

private:
    void handleCurrentTabChanged();
    void setBookmarksTabActive(bool value);

private:
    EventPanel::Private* const m_main;
    QWidget* m_revertTab = nullptr;
    QWidget* m_lastTab = nullptr;
};

} // namespace nx::client::desktop
