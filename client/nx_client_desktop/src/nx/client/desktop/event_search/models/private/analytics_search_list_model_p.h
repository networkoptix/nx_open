#pragma once

#include "../analytics_search_list_model.h"

#include <chrono>
#include <deque>
#include <limits>
#include <memory>
#include <vector>

#include <QtCore/QSet>
#include <QtCore/QHash>
#include <QtCore/QSharedPointer>

#include <api/server_rest_connection_fwd.h>
#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/camera/camera_fwd.h>
#include <nx/client/desktop/event_search/models/private/abstract_async_search_list_model_p.h>
#include <nx/client/desktop/event_search/utils/live_analytics_receiver.h>
#include <nx/media/signaling_metadata_consumer.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

class QnUuid;
class QnMediaResourceWidget;
class QMenu;

namespace nx::utils { class PendingOperation; }

namespace nx::client::desktop {

class AnalyticsSearchListModel::Private: public AbstractAsyncSearchListModel::Private
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel::Private;

public:
    explicit Private(AnalyticsSearchListModel* q);
    virtual ~Private() override;

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
    void updateMetadataReceivers();
    void processMetadata();

    int indexOf(const QnUuid& objectId) const;

    template<typename Iter>
    bool commitInternal(const QnTimePeriod& periodToCommit, Iter prefetchBegin, Iter prefetchEnd,
        int position, bool handleOverlaps);

    void emitDataChangedIfNeeded();
    void advanceObject(analytics::storage::DetectedObject& object,
        analytics::storage::ObjectPosition&& position, bool emitDataChanged = true);

    using GetCallback = std::function<void(bool, rest::Handle, analytics::storage::LookupResult&&)>;
    rest::Handle getObjects(const QnTimePeriod& period, GetCallback callback, int limit);

    QString description(const analytics::storage::DetectedObject& object) const;
    QString attributes(const analytics::storage::DetectedObject& object) const;
    QSharedPointer<QMenu> contextMenu(const analytics::storage::DetectedObject& object) const;

    QnVirtualCameraResourcePtr camera(const analytics::storage::DetectedObject& object) const;

    void executePluginAction(const QString& pluginId,
        const nx::vms::api::analytics::PluginManifest::ObjectAction& action,
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

    std::vector<std::unique_ptr<LiveAnalyticsReceiver>> m_metadataReceivers;
    const QScopedPointer<QTimer> m_metadataProcessingTimer;

    analytics::storage::LookupResult m_prefetch;
    std::deque<analytics::storage::DetectedObject> m_data;

    QSet<QnUuid> m_dataChangedObjectIds; //< For which objects delayed dataChanged is queued.
    QHash<QnUuid, std::chrono::milliseconds> m_objectIdToTimestamp;
};

} // namespace nx::client::desktop
