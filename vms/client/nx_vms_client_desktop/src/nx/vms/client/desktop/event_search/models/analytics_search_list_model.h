#pragma once

#include <functional>

#include <QtCore/QRectF>

#include <nx/vms/client/desktop/event_search/models/abstract_async_search_list_model.h>

namespace nx::analytics::db { struct ObjectTrack; }

namespace nx::vms::client::desktop {

class AnalyticsSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit AnalyticsSearchListModel(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~AnalyticsSearchListModel() override = default;

    QRectF filterRect() const;
    void setFilterRect(const QRectF& relativeRect);

    QString filterText() const;
    void setFilterText(const QString& value);

    QString selectedObjectType() const;
    void setSelectedObjectType(const QString& value);

    // Metadata newer than timestamp returned by this callback will be deferred.
    using LiveTimestampGetter = std::function<
        std::chrono::milliseconds(const QnVirtualCameraResourcePtr&)>;
    void setLiveTimestampGetter(LiveTimestampGetter value);

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

signals:
    void pluginActionRequested(const QnUuid& engineId, const QString& actionTypeId,
        const analytics::db::ObjectTrack& track, const QnVirtualCameraResourcePtr& camera,
        QPrivateSignal);

protected:
    virtual bool isCameraApplicable(const QnVirtualCameraResourcePtr& camera) const override;

private:
    class Private;
    Private* const d;
};

} // namespace nx::vms::client::desktop
