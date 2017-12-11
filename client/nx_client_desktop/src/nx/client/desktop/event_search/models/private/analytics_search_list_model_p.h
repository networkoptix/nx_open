#pragma once

#include "../analytics_search_list_model.h"

#include <deque>
#include <limits>

#include <QtCore/QHash>

#include <api/server_rest_connection_fwd.h>
#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/camera/camera_fwd.h>
#include <nx/media/signaling_metadata_consumer.h>

class QnUuid;
class QnMediaResourceWidget;

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
    const analytics::storage::DetectedObject& object(int index) const;

    void setFilterRect(const QRectF& relativeRect);

    void clear();

    bool canFetchMore() const;
    bool prefetch(PrefetchCompletionHandler completionHandler);
    void commitPrefetch(qint64 latestStartTimeMs);

    QString description(const analytics::storage::DetectedObject& object) const;
    static qint64 startTimeMs(const analytics::storage::DetectedObject& object);

private:
    void processMetadata(const QnAbstractCompressedMetadataPtr& metadata);
    media::SignalingMetadataConsumer* createMetadataSource();

    int indexOf(const QnUuid& objectId, qint64 timeUs) const;

    void periodicUpdate();
    void addNewlyReceivedObjects(analytics::storage::LookupResult&& data);

    using GetCallback = std::function<void(bool, rest::Handle, analytics::storage::LookupResult&&)>;
    rest::Handle getObjects(qint64 startMs, qint64 endMs, GetCallback callback,
        int limit = std::numeric_limits<int>::max());

private:
    AnalyticsSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    QRectF m_filterRect;
    QScopedPointer<QTimer> m_updateTimer;
    QPointer<QTimer> m_dataChangedTimer;
    media::AbstractMetadataConsumerPtr m_metadataSource;
    QnResourceDisplayPtr m_display;
    qint64 m_latestTimeMs = 0;
    qint64 m_earliestTimeMs = std::numeric_limits<qint64>::max();
    rest::Handle m_currentFetchId = rest::Handle();
    rest::Handle m_currentUpdateId = rest::Handle();
    bool m_fetchedAll = false;
    bool m_success = true;

    analytics::storage::LookupResult m_prefetch;
    std::deque<analytics::storage::DetectedObject> m_data;

    QHash<QnUuid, qint64> m_lastObjectTimesUs;
};

} // namespace
} // namespace client
} // namespace nx
