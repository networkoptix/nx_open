#include "notification_list_widget.h"
#include "private/notification_list_widget_p.h"

namespace nx {
namespace client {
namespace desktop {

NotificationListWidget::NotificationListWidget(QWidget* parent):
    QWidget(parent),
    d(new Private(this))
{
}

NotificationListWidget::~NotificationListWidget()
{
}

} // namespace
} // namespace client
} // namespace nx
