#include "notification_list_widget.h"
#include "private/notification_list_widget_p.h"

#include <QtCore/QEvent>
#include <QtGui/QMouseEvent>

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

void NotificationListWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress && event->button() == Qt::RightButton)
        event->accept();
}

} // namespace desktop
} // namespace client
} // namespace nx
