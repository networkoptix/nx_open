#include "notification_list_model.h"
#include "private/notification_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

NotificationListModel::NotificationListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

NotificationListModel::~NotificationListModel()
{
}

} // namespace
} // namespace client
} // namespace nx
