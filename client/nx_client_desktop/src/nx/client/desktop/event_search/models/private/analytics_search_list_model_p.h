#pragma once

#include "../analytics_search_list_model.h"

#include <deque>
#include <limits>

#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <core/resource/resource_fwd.h>

class QnUuid;

namespace nx {
namespace client {
namespace desktop {

class AnalyticsSearchListModel::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(AnalyticsSearchListModel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    int count() const;

    void clear();

    bool canFetchMore() const;
    bool prefetch(PrefetchCompletionHandler completionHandler);
    void commitPrefetch(qint64 latestStartTimeMs);

private:
    AnalyticsSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    qint64 m_earliestTimeMs = std::numeric_limits<qint64>::max();
    bool m_fetchedAll = false;
    bool m_success = true;

    analytics::storage::LookupResult m_prefetch;
    std::deque<analytics::storage::DetectedObject> m_data;
};

} // namespace
} // namespace client
} // namespace nx
