// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

#include <QtCore/QHash>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtCore/QTimer>

#include <analytics/db/analytics_db_types.h>
#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/camera/camera_fwd.h>
#include <nx/vms/client/desktop/event_search/models/private/abstract_async_search_list_model_p.h>
#include <nx/vms/client/desktop/event_search/utils/live_analytics_receiver.h>
#include <nx/vms/client/desktop/event_search/utils/text_filter_setup.h>

#include "../analytics_search_list_model.h"

class QnUuid;
class QnMediaResourceWidget;
class QMenu;
class QJsonObject;

namespace nx::utils { class PendingOperation; }

namespace nx::vms::client::desktop {

class AnalyticsSearchListModel::Private:
    public AbstractAsyncSearchListModel::Private,
    public core::RemoteConnectionAware
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel::Private;

    AnalyticsSearchListModel* const q;

public:
    const std::unique_ptr<TextFilterSetup> textFilter{new TextFilterSetup()};

public:
    explicit Private(AnalyticsSearchListModel* q);
    virtual ~Private() override;

    virtual int count() const override;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const override;

    QRectF filterRect() const;
    void setFilterRect(const QRectF& relativeRect);

    QnUuid selectedEngine() const;
    void setSelectedEngine(const QnUuid& value);

    QStringList selectedObjectTypes() const;
    void setSelectedObjectTypes(const QStringList& value);
    const std::set<QString>& relevantObjectTypes() const;

    QStringList attributeFilters() const;
    void setAttributeFilters(const QStringList& value);

    void setLiveTimestampGetter(LiveTimestampGetter value);

    LiveProcessingMode liveProcessingMode() const;
    void setLiveProcessingMode(LiveProcessingMode value);

    int availableNewTracks() const;
    void commitNewTracks();

    virtual void clearData() override;
    virtual void truncateToMaximumCount() override;
    virtual void truncateToRelevantTimePeriod() override;

    bool canViewArchive(const QnVirtualCameraResourcePtr& camera) const;

    QString makeEnumValuesExact(const QString& filter);

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) override;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit) override;

private:
    void updateMetadataReceivers();
    void processMetadata();

    void updateRelevantObjectTypes();

    template<typename Iter>
    bool commitInternal(const QnTimePeriod& periodToCommit, Iter prefetchBegin, Iter prefetchEnd,
        int position, bool handleOverlaps);

    void emitDataChangedIfNeeded();
    void advanceTrack(nx::analytics::db::ObjectTrack& track,
        nx::analytics::db::ObjectPosition&& position, bool emitDataChanged = true);

    using GetCallback = std::function<void(bool, rest::Handle, nx::analytics::db::LookupResult&&)>;
    rest::Handle getObjects(const QnTimePeriod& period, GetCallback callback, int limit) const;
    rest::Handle lookupObjectTracksCached(
        const nx::analytics::db::Filter& request,
        GetCallback callback) const;

    const nx::analytics::taxonomy::AbstractObjectType* objectTypeById(
        const QString& objectTypeId) const;

    QString iconPath(const QString& objectTypeId) const;
    QString description(const nx::analytics::db::ObjectTrack& track) const;
    QString engineName(const nx::analytics::db::ObjectTrack& track) const;
    QSharedPointer<QMenu> contextMenu(const nx::analytics::db::ObjectTrack& track) const;

    QnVirtualCameraResourcePtr camera(const nx::analytics::db::ObjectTrack& track) const;

    nx::utils::Guard makeAvailableNewTracksGuard();

    void setLiveReceptionActive(bool value);

    bool isAcceptedObjectType(const QString& typeId) const;

    struct PreviewParams
    {
        std::chrono::microseconds timestamp = std::chrono::microseconds(0);
        QRectF boundingBox;
    };

    static PreviewParams previewParams(const nx::analytics::db::ObjectTrack& track);

    struct Storage
    {
        std::deque<nx::analytics::db::ObjectTrack> items; //< Descending sort order.
        QHash<QnUuid, std::chrono::milliseconds> idToTimestamp;

        int size() const { return (int)items.size(); }
        bool empty() const { return items.empty(); }
        int indexOf(const QnUuid& trackId) const;
        nx::analytics::db::ObjectTrack take(int index);
        void clear();

        // This function handles single item insertion. It returns index of inserted item.
        // Block insertion is handled outside.
        int insert(nx::analytics::db::ObjectTrack&& item,
            std::function<void(int index)> notifyBeforeInsertion = nullptr,
            std::function<void()> notifyAfterInsertion = nullptr);
    };

    ItemCleanupFunction<nx::analytics::db::ObjectTrack> itemCleanupFunction(Storage& storage);

private:
    QRectF m_filterRect;
    QnUuid m_selectedEngine;
    QStringList m_selectedObjectTypes;
    std::set<QString> m_relevantObjectTypes; //< Empty for all types.
    mutable QHash<QString, bool> m_objectTypeAcceptanceCache; //< Used in metadata processing.

    QStringList m_attributeFilters;

    const QScopedPointer<nx::utils::PendingOperation> m_emitDataChanged;
    bool m_liveReceptionActive = false;

    using MetadataReceiverList = std::vector<std::unique_ptr<LiveAnalyticsReceiver>>;
    MetadataReceiverList m_metadataReceivers;
    const QScopedPointer<QTimer> m_metadataProcessingTimer;

    nx::analytics::db::LookupResult m_prefetch;

    Storage m_data;
    QSet<QnUuid> m_externalBestShotTracks;

    QSet<QnUuid> m_dataChangedTrackIds; //< For which tracks delayed dataChanged is queued.

    LiveProcessingMode m_liveProcessingMode = LiveProcessingMode::automaticAdd;

    LiveTimestampGetter m_liveTimestampGetter;
    QHash<QnVirtualCameraResourcePtr, QList<QnAbstractCompressedMetadataPtr>> m_deferredMetadata;

    Storage m_newTracks;
    Storage m_noBestShotTracks;
    bool m_gapBeforeNewTracks = false;
    bool m_newTrackCountUnknown = false;
    bool m_keepLiveDataAtClear = false;
};

} // namespace nx::vms::client::desktop
