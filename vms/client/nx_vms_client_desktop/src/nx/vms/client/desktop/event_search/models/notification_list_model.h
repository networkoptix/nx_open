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

protected:
    virtual bool defaultAction(const QModelIndex& index) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
