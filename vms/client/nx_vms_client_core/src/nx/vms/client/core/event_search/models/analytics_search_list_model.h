// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <set>

#include <QtCore/QRectF>

#include <nx/vms/client/core/event_search/models/abstract_async_search_list_model.h>

namespace nx::analytics::db { struct ObjectTrack; }

namespace nx::vms::client::core {

class TextFilterSetup;
class SystemContext;

class NX_VMS_CLIENT_CORE_API AnalyticsSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit AnalyticsSearchListModel(
        SystemContext* systemContex,
        int maximumCount,
        QObject* parent = nullptr);
    explicit AnalyticsSearchListModel(SystemContext* systemContex, QObject* parent = nullptr);
    virtual ~AnalyticsSearchListModel() override;

    virtual void setSystemContext(SystemContext* systemContext) override;

    QRectF filterRect() const;
    void setFilterRect(const QRectF& relativeRect);

    virtual TextFilterSetup* textFilter() const override;

    nx::Uuid selectedEngine() const;
    void setSelectedEngine(const nx::Uuid& value);

    QStringList selectedObjectTypes() const;
    void setSelectedObjectTypes(const QStringList& value);

    QStringList attributeFilters() const;
    void setAttributeFilters(const QStringList& value);

    QString combinedTextFilter() const; // Free text filter combined with attribute filters.

    // Metadata newer than timestamp returned by this callback will be deferred.
    using LiveTimestampGetter = std::function<
        std::chrono::milliseconds(const QnVirtualCameraResourcePtr&)>;
    void setLiveTimestampGetter(LiveTimestampGetter value);

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

    enum class LiveProcessingMode
    {
        automaticAdd, //< Add to the head if we see top of the list.
        manualAdd //< Manual mode where we add items manually by user request (like button click).
    };
    Q_ENUM(LiveProcessingMode)

    LiveProcessingMode liveProcessingMode() const;
    void setLiveProcessingMode(LiveProcessingMode value);
    bool hasOnlyLiveCameras() const;

    // Methods for `LiveProcessingMode::manualAdd` mode.
    int availableNewTracks() const;
    void commitAvailableNewTracks();
    static constexpr int kUnknownAvailableTrackCount = -1;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

protected:
    virtual bool requestFetch(
        const FetchRequest& request,
        const FetchCompletionHandler& completionHandler) override;

    virtual void clearData() override;

    const nx::analytics::db::ObjectTrack& trackByRow(int row) const;

    QnVirtualCameraResourcePtr camera(const nx::analytics::db::ObjectTrack& track) const;

    struct PreviewParams
    {
        std::chrono::microseconds timestamp = {};
        QRectF boundingBox;
    };

    PreviewParams previewParams(const nx::analytics::db::ObjectTrack& track) const;

signals:
    void filterRectChanged();
    void selectedEngineChanged();
    void selectedObjectTypesChanged();
    void availableNewTracksChanged();
    void attributeFiltersChanged();
    void combinedTextFilterChanged();

protected:
    virtual bool isFilterDegenerate() const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
