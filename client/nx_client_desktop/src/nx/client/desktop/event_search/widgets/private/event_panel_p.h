#pragma once

#include "../event_panel.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

#include <nx/utils/disconnect_helper.h>

class QTabWidget;
class QStackedWidget;
class QnMediaResourceWidget;

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

private:
    EventPanel* q = nullptr;
    QTabWidget* m_tabs = nullptr;

    NotificationListWidget* m_notificationsTab = nullptr;
    MotionSearchWidget* m_motionTab = nullptr;
    UnifiedSearchWidget* m_bookmarksTab = nullptr;
    UnifiedSearchWidget* m_eventsTab = nullptr;
    UnifiedSearchWidget* m_analyticsTab = nullptr;

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
