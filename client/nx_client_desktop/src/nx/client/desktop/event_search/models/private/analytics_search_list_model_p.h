#pragma once

#include "../analytics_search_list_model.h"

#include <deque>
#include <limits>

#include <QtCore/QHash>

#include <api/server_rest_connection_fwd.h>
#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/camera/camera_fwd.h>
#include <nx/client/desktop/event_search/models/private/abstract_async_search_list_model_p.h>
#include <nx/media/signaling_metadata_consumer.h>

class QnUuid;
class QnMediaResourceWidget;

namespace nx {
namespace client {
namespace desktop {

class AnalyticsSearchListModel::Private: public AbstractAsyncSearchListModel::Private
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel::Private;

public:
    explicit Private(AnalyticsSearchListModel* q);
    virtual ~Private() override;

    virtual void setCamera(const QnVirtualCameraResourcePtr& camera) override;

    virtual int count() const override;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const override;

    QRectF filterRect() const;
    void setFilterRect(const QRectF& relativeRect);

    virtual void clear() override;

    bool defaultAction(int index) const;

protected:
    virtual rest::Handle requestPrefetch(qint64 latestTimeMs) override;
    virtual bool commitPrefetch(qint64 earliestTimeToCommitMs, bool& fetchedAll) override;
    virtual bool hasAccessRights() const override;

private:
    void processMetadata(const QnAbstractCompressedMetadataPtr& metadata);
    media::SignalingMetadataConsumer* createMetadataSource();

    int indexOf(const QnUuid& objectId, qint64 timeUs) const;

    void periodicUpdate();
    void addNewlyReceivedObjects(analytics::storage::LookupResult&& data);

    using GetCallback = std::function<void(bool, rest::Handle, analytics::storage::LookupResult&&)>;
    rest::Handle getObjects(qint64 startMs, qint64 endMs, GetCallback callback,
        int limit = std::numeric_limits<int>::max());

    QString description(const analytics::storage::DetectedObject& object) const;
    static qint64 startTimeMs(const analytics::storage::DetectedObject& object);

private:
    AnalyticsSearchListModel* const q = nullptr;
    QRectF m_filterRect;
    QScopedPointer<QTimer> m_updateTimer;
    QPointer<QTimer> m_dataChangedTimer;
    media::AbstractMetadataConsumerPtr m_metadataSource;
    QnResourceDisplayPtr m_display;
    qint64 m_latestTimeMs = 0;
    rest::Handle m_currentUpdateId = rest::Handle();

    analytics::storage::LookupResult m_prefetch;
    std::deque<analytics::storage::DetectedObject> m_data;
    bool m_success = true;

    QHash<QnUuid, qint64> m_lastObjectTimesUs;
};

} // namespace
} // namespace client
} // namespace nx
