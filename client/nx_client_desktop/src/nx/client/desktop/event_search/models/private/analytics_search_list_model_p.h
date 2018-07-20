#pragma once

#include "../analytics_search_list_model.h"

#include <chrono>
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

    virtual void clearData() override;
    virtual void truncateToMaximumCount() override;
    virtual void truncateToRelevantTimePeriod() override;

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) override;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit) override;
    virtual bool hasAccessRights() const override;

private:
    void processMetadata();
    media::SignalingMetadataConsumer* createMetadataSource();

    int indexOf(const QnUuid& objectId) const;

    template<typename Iter>
    bool commitPrefetch(
        const QnTimePeriod& periodToCommit, Iter prefetchBegin, Iter prefetchEnd, int position);

    void emitDataChangedIfNeeded();
    void advanceObject(analytics::storage::DetectedObject& object,
        analytics::storage::ObjectPosition&& position, bool emitDataChanged = true);

    using GetCallback = std::function<void(bool, rest::Handle, analytics::storage::LookupResult&&)>;
    rest::Handle getObjects(const QnTimePeriod& period, GetCallback callback, int limit);

    QString description(const analytics::storage::DetectedObject& object) const;
    QString attributes(const analytics::storage::DetectedObject& object) const;
    QSharedPointer<QMenu> contextMenu(const analytics::storage::DetectedObject& object) const;

    void executePluginAction(const QnUuid& driverId,
        const api::AnalyticsManifestObjectAction& action,
        const analytics::storage::DetectedObject& object) const;

    struct PreviewParams
    {
        std::chrono::microseconds timestamp = std::chrono::microseconds(0);
        QRectF boundingBox;
    };

    static PreviewParams previewParams(const analytics::storage::DetectedObject& object);

private:
    AnalyticsSearchListModel* const q = nullptr;
    QRectF m_filterRect;
    QString m_filterText;
    const QScopedPointer<utils::PendingOperation> m_emitDataChanged;
    const QScopedPointer<utils::PendingOperation> m_updateWorkbenchFilter;
    QSet<QnUuid> m_dataChangedObjectIds; //< For which objects delayed dataChanged is queued.
    media::AbstractMetadataConsumerPtr m_metadataSource;
    QnResourceDisplayPtr m_display;

    analytics::storage::LookupResult m_prefetch;
    std::deque<analytics::storage::DetectedObject> m_data;

    QHash<QnUuid, std::chrono::milliseconds> m_objectIdToTimestamp;

    const QScopedPointer<QTimer> m_metadataProcessingTimer;
    QVector<QnAbstractCompressedMetadataPtr> m_metadataPackets;
    mutable QnMutex m_metadataMutex;
};

} // namespace desktop
} // namespace client
} // namespace nx
