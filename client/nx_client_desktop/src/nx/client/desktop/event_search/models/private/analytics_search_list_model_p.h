#pragma once

#include "../analytics_search_list_model.h"

#include <deque>
#include <limits>

#include <QtCore/QSet>
#include <QtCore/QHash>
#include <QtCore/QSharedPointer>

#include <api/server_rest_connection_fwd.h>
#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/camera/camera_fwd.h>
#include <nx/client/desktop/event_search/models/private/abstract_async_search_list_model_p.h>
#include <nx/media/signaling_metadata_consumer.h>

class QnUuid;
class QnMediaResourceWidget;
class QMenu;

namespace nx {

namespace api { struct AnalyticsManifestObjectAction; }

namespace utils { class PendingOperation; }

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

    QString filterText() const;
    void setFilterText(const QString& value);

    virtual void clear() override;

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) override;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit, bool& fetchedAll) override;
    virtual void clipToSelectedTimePeriod() override;
    virtual bool hasAccessRights() const override;

private:
    void processMetadata(const QnAbstractCompressedMetadataPtr& metadata);
    media::SignalingMetadataConsumer* createMetadataSource();

    int indexOf(const QnUuid& objectId) const;

    void periodicUpdate();
    void refreshUpdateTimer();
    void addNewlyReceivedObjects(analytics::storage::LookupResult&& data);
    void emitDataChangedIfNeeded();

    void advanceObject(analytics::storage::DetectedObject& object,
        analytics::storage::ObjectPosition&& position);

    using GetCallback = std::function<void(bool, rest::Handle, analytics::storage::LookupResult&&)>;
    rest::Handle getObjects(const QnTimePeriod& period, GetCallback callback,
        int limit = std::numeric_limits<int>::max());

    QString description(const analytics::storage::DetectedObject& object) const;
    QString attributes(const analytics::storage::DetectedObject& object) const;
    QSharedPointer<QMenu> contextMenu(const analytics::storage::DetectedObject& object) const;

    utils::PendingOperation* createUpdateWorkbenchFilterOperation();

    void executePluginAction(const QnUuid& driverId,
        const api::AnalyticsManifestObjectAction& action,
        const analytics::storage::DetectedObject& object) const;

private:
    AnalyticsSearchListModel* const q = nullptr;
    QRectF m_filterRect;
    QString m_filterText;
    const QScopedPointer<QTimer> m_updateTimer;
    const QScopedPointer<QTimer> m_dataChangedTimer;
    const QScopedPointer<utils::PendingOperation> m_updateWorkbenchFilter;
    QSet<QnUuid> m_dataChangedObjectIds; //< For which objects delayed dataChanged is queued.
    media::AbstractMetadataConsumerPtr m_metadataSource;
    QnResourceDisplayPtr m_display;
    qint64 m_latestTimeMs = 0;
    rest::Handle m_currentUpdateId = rest::Handle();

    analytics::storage::LookupResult m_prefetch;
    std::deque<analytics::storage::DetectedObject> m_data;
    bool m_success = true;

    QHash<QnUuid, qint64> m_objectIdToTimestampMs;
};

} // namespace desktop
} // namespace client
} // namespace nx
