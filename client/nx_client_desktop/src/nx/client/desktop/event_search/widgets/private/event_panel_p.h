#pragma once

#include "../event_panel.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

#include <nx/utils/disconnect_helper.h>

class QTabWidget;
class QStackedWidget;
class QnMediaResourceWidget;

namespace QnNotificationLevel { enum class Value; }

namespace nx {

namespace vms { namespace event { class StringsHelper; } }

namespace client {
namespace desktop {

class EventSearchListModel;
class BookmarkSearchListModel;
class AnalyticsSearchListModel;

class NotificationListWidget;
class UnifiedSearchWidget;
class MotionSearchWidget;
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

    void addCameraTabs();
    void removeCameraTabs();

    void setupEventSearch();
    void setupBookmarkSearch();
    void setupAnalyticsSearch();

    void updateUnreadCounter(int count, QnNotificationLevel::Value importance);

private:
    EventPanel* const q = nullptr;
    QTabWidget* const m_tabs = nullptr;

    NotificationListWidget* const m_notificationsTab = nullptr;
    MotionSearchWidget* const m_motionTab = nullptr;
    UnifiedSearchWidget* const m_bookmarksTab = nullptr;
    UnifiedSearchWidget* const m_eventsTab = nullptr;
    UnifiedSearchWidget* const m_analyticsTab = nullptr;
    NotificationCounterLabel* const m_counterLabel = nullptr;

    QPointer<QnMediaResourceWidget> m_currentMediaWidget;
    QScopedPointer<QnDisconnectHelper> m_mediaWidgetConnections;

    QnVirtualCameraResourcePtr m_camera;

    EventSearchListModel* const m_eventsModel = nullptr;
    BookmarkSearchListModel* const m_bookmarksModel = nullptr;
    AnalyticsSearchListModel* const m_analyticsModel = nullptr;

    QScopedPointer<vms::event::StringsHelper> m_helper;
};

} // namespace desktop
} // namespace client
} // namespace nx
