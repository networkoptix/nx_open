#include "notification_list_model_p.h"

#include <business/business_resource_validation.h>
#include <client/client_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

NotificationListModel::Private::Private(NotificationListModel* q):
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    const auto handler = context()->instance<QnWorkbenchNotificationsHandler>();
    connect(handler, &QnWorkbenchNotificationsHandler::cleared, q, &EventListModel::clear);
    connect(handler, &QnWorkbenchNotificationsHandler::notificationAdded,
        this, &Private::addNotification);
    connect(handler, &QnWorkbenchNotificationsHandler::notificationRemoved,
        this, &Private::removeNotification);
}

NotificationListModel::Private::~Private()
{
}

void NotificationListModel::Private::addNotification(const vms::event::AbstractActionPtr& action)
{
}

void NotificationListModel::Private::removeNotification(const vms::event::AbstractActionPtr& action)
{

}

} // namespace
} // namespace client
} // namespace nx
