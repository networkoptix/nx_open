#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/event_search/models/abstract_event_list_model.h>
#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class EventSearchListModel: public AbstractEventListModel
{
    Q_OBJECT
    using base_type = AbstractEventListModel;

public:
    explicit EventSearchListModel(QObject* parent = nullptr);
    virtual ~EventSearchListModel() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    vms::event::EventType selectedEventType() const;
    void setSelectedEventType(vms::event::EventType value);

    void clear();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual bool prefetchAsync(PrefetchCompletionHandler completionHandler) override;
    virtual void commitPrefetch(qint64 keyLimitFromSync) override;
    virtual bool fetchInProgress() const override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
