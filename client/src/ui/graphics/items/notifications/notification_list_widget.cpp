#include "notification_list_widget.h"

#include <ui/graphics/items/notifications/notification_item.h>

QnNotificationListWidget::QnNotificationListWidget(QGraphicsItem *parent, Qt::WindowFlags flags):
    base_type(parent, flags) {
}

QnNotificationListWidget::~QnNotificationListWidget() {
}

void QnNotificationListWidget::addItem(QnNotificationItem *item)  {
    connect(item, SIGNAL(geometryChanged()), this, SLOT(at_item_geometryChanged()));
    item->setText(QString::number(m_items.size()));
    m_items.append(item);
    at_item_geometryChanged();
}

void QnNotificationListWidget::at_item_geometryChanged() {
    int top = 0;
    foreach (QnNotificationItem* item, m_items) {
        qDebug() << "set item pos" << item->text() << "y =" << top;
        item->setY(top);
        top += item->geometry().height();
    }
}
