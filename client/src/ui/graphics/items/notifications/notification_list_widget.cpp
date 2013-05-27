#include "notification_list_widget.h"

#include <ui/graphics/items/notifications/notification_item.h>

QnNotificationListWidget::QnNotificationListWidget(QGraphicsItem *parent, Qt::WindowFlags flags):
    base_type(parent, flags)
{
    registerAnimation(this);
    startListening();
}

QnNotificationListWidget::~QnNotificationListWidget() {
}

QSizeF QnNotificationListWidget::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    qreal d = std::numeric_limits<qreal>::max() / 2;

    if (which == Qt::MaximumSize) {
        if (constraint.isNull())
            return QSizeF(d, d);
        return constraint;
    }

    if (m_items.isEmpty())
        return QSizeF();

    QSizeF result;
    foreach (QnNotificationItem* item, m_items) {
        QSizeF itemSize = item->geometry().size();
        if (itemSize.isNull())
            continue;
        result.setWidth(qMax(result.width(), itemSize.width()));
        result.setHeight(result.height() + itemSize.height());
    }
    return result;
}

void QnNotificationListWidget::tick(int deltaMSecs) {
 //   qDebug() << "tick" << deltaMSecs;
}

void QnNotificationListWidget::addItem(QnNotificationItem *item)  {
    connect(item, SIGNAL(geometryChanged()), this, SLOT(at_item_geometryChanged()));
    item->setText(QLatin1String("Motion detected on camera\nblablabla") + QString::number(m_items.size()));
    item->setZValue(-1* m_items.size());
    m_items.append(item);
    at_item_geometryChanged();
}

void QnNotificationListWidget::at_item_geometryChanged() {
    prepareGeometryChange();
    int top = 0;
    foreach (QnNotificationItem* item, m_items) {
        item->setY(top);
        top += item->geometry().height();
    }
    updateGeometry();
}
