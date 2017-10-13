#include "system_health_list_model_p.h"

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>

#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

SystemHealthListModel::Private::Private(SystemHealthListModel* q):
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    const auto handler = context()->instance<QnWorkbenchNotificationsHandler>();
    connect(handler, &QnWorkbenchNotificationsHandler::cleared, q, &EventListModel::clear);
    connect(handler, &QnWorkbenchNotificationsHandler::systemHealthEventAdded,
        this, &Private::addSystemHealthEvent);
    connect(handler, &QnWorkbenchNotificationsHandler::systemHealthEventRemoved,
        this, &Private::removeSystemHealthEvent);
}

SystemHealthListModel::Private::~Private()
{
}

void SystemHealthListModel::Private::addSystemHealthEvent(
    QnSystemHealth::MessageType message,
    const QVariant& params)
{
    //ScopedInsertRows insertRows(q, QModelIndex(), 0, 0);

}

void SystemHealthListModel::Private::removeSystemHealthEvent(
    QnSystemHealth::MessageType message,
    const QVariant& params)
{
    //ScopedRemoveRows removeRows(q, QModelIndex(), i, i);

}

} // namespace
} // namespace client
} // namespace nx
