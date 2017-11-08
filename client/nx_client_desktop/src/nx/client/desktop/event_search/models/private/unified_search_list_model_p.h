#pragma once

#include "../unified_search_list_model.h"

#include <QtCore/QHash>
#include <QtCore/QTimer>
#include <QtCore/QWeakPointer>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <nx/utils/raii_guard.h>
#include <nx/vms/event/event_fwd.h>

class QnRaiiGuard;

namespace nx {
namespace client {
namespace desktop {

class UnifiedSearchListModel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(UnifiedSearchListModel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    void clear();

    bool canFetchMore() const;
    void fetchMore();

private:
    void fetchAll(qint64 startMs, qint64 endMs, QnRaiiGuardPtr handler = QnRaiiGuardPtr());
    void fetchEvents(qint64 startMs, qint64 endMs, QnRaiiGuardPtr handler = QnRaiiGuardPtr());
    void fetchBookmarks(qint64 startMs, qint64 endMs, QnRaiiGuardPtr handler = QnRaiiGuardPtr());
    void fetchAnalytics(qint64 startMs, qint64 endMs, QnRaiiGuardPtr handler = QnRaiiGuardPtr());

    void periodicUpdate();
    void watchBookmarkChanges();

    void addOrUpdateBookmark(const QnCameraBookmark& bookmark);
    void addCameraEvent(const nx::vms::event::ActionData& event);

    static QPixmap eventPixmap(const nx::vms::event::EventParameters& event);
    static QColor eventColor(nx::vms::event::EventType eventType);
    static bool eventRequiresPreview(vms::event::EventType type);

private:
    UnifiedSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<QTimer> m_updateTimer;
    QScopedPointer<vms::event::StringsHelper> m_helper;
    QHash<rest::Handle, QnRaiiGuardPtr> m_eventRequests;
    QHash<QPair<QnUuid, qint64>, QnUuid> m_eventIds;
    qint64 m_startTimeMs = 0;
    qint64 m_endTimeMs = -1;
    QWeakPointer<QnRaiiGuard> m_fetchInProgress;
};

} // namespace
} // namespace client
} // namespace nx
