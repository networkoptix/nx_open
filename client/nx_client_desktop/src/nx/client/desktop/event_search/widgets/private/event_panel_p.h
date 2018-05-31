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

namespace nx {

namespace vms { namespace event { class StringsHelper; } }

namespace client {
namespace desktop {

class EventSearchListModel;
class MotionSearchListModel;
class BookmarkSearchListModel;
class AnalyticsSearchListModel;

class NotificationListWidget;
class UnifiedSearchWidget;
class NotificationCounterLabel;

class EventPanel::Private: public QObject
{
    Q_OBJECT

public:
    Private(EventPanel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

private:
    void currentWorkbenchWidgetChanged(Qn::ItemRole role);
    void updateTabs();

    void setupMotionSearch();
    void setupBookmarkSearch();
    void setupEventSearch();
    void setupAnalyticsSearch();

    void updateUnreadCounter(int count, QnNotificationLevel::Value importance);

    void setupTabsSyncWithNavigator();

    void connectToRowCountChanges(QAbstractItemModel* model, std::function<void()> handler);

    void at_motionSearchToggled(bool on);
    void at_bookmarksToggled(bool on);

    void at_specialModeToggled(bool on, QWidget* correspondingTab);

    void at_motionSelectionChanged();

private:
    EventPanel* const q = nullptr;
    QTabWidget* const m_tabs = nullptr;

    NotificationListWidget* const m_notificationsTab = nullptr;
    UnifiedSearchWidget* const m_motionTab = nullptr;
    UnifiedSearchWidget* const m_bookmarksTab = nullptr;
    UnifiedSearchWidget* const m_eventsTab = nullptr;
    UnifiedSearchWidget* const m_analyticsTab = nullptr;
    NotificationCounterLabel* const m_counterLabel = nullptr;

    QPointer<QnMediaResourceWidget> m_currentMediaWidget;
    QScopedPointer<QnDisconnectHelper> m_mediaWidgetConnections;

    QnVirtualCameraResourcePtr m_camera;

    EventSearchListModel* const m_eventsModel = nullptr;
    MotionSearchListModel* const m_motionModel = nullptr;
    BookmarkSearchListModel* const m_bookmarksModel = nullptr;
    AnalyticsSearchListModel* const m_analyticsModel = nullptr;

    QScopedPointer<vms::event::StringsHelper> m_helper;
    int m_previousTabIndex = 0;
    int m_lastTabIndex = 0;
};

} // namespace desktop
} // namespace client
} // namespace nx
