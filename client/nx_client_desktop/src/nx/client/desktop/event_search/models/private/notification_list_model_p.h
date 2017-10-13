#pragma once

#include "../notification_list_model.h"

#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class NotificationListModel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(NotificationListModel* q);
    virtual ~Private() override;

private:
    void addNotification(const vms::event::AbstractActionPtr& action);
    void removeNotification(const vms::event::AbstractActionPtr& action);

private:
    NotificationListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
};

} // namespace
} // namespace client
} // namespace nx
