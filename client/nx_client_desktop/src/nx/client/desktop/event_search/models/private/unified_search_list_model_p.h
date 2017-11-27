#pragma once

#include "../unified_search_list_model.h"

#include <limits>

#include <QtCore/QWeakPointer>

#include <core/resource/resource_fwd.h>

#include <nx/utils/raii_guard.h>

class QnRaiiGuard;

namespace nx {
namespace client {
namespace desktop {

class EventSearchListModel;
class BookmarkSearchListModel;
class AnalyticsSearchListModel;

class UnifiedSearchListModel::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(UnifiedSearchListModel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    Types filter() const;
    void setFilter(Types filter);

    void clear();

    bool canFetchMore() const;
    void fetchMore();

private:
    void updateModels();

private:
    UnifiedSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    Types m_filter = Type::all;

    EventSearchListModel* const m_eventsModel;
    BookmarkSearchListModel* const m_bookmarksModel;
    AnalyticsSearchListModel* const m_analyticsModel;

    qint64 m_earliestTimeMs = std::numeric_limits<qint64>::max();
    qint64 m_latestTimeMs = -1;

    QWeakPointer<QnRaiiGuard> m_fetchInProgress;
    Types m_fetchingTypes;
    qint64 m_latestStartTimeMs = -1; //< Synchronization point used during fetch.
};

} // namespace
} // namespace client
} // namespace nx
