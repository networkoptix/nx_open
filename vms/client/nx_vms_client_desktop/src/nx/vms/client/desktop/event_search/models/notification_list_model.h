#pragma once

#include <QtCore/QScopedPointer>

#include <nx/vms/client/desktop/event_search/models/event_list_model.h>

namespace nx::vms::client::desktop {

class NotificationListModel: public EventListModel
{
    Q_OBJECT
    using base_type = EventListModel;

public:
    explicit NotificationListModel(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~NotificationListModel() override;

    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
