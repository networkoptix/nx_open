#include "notification_list_widget.h"

#include <QtCore/QDateTime>

#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/processors/hover_processor.h>

#include <ui/style/globals.h>

namespace {

    int popoutTimeoutMs = 300;
    int moveUpTimeoutMs = 500;
    int displayTimeoutMs = 5000;
    int hideTimeoutMs = 300;
    int collapseTimeoutMs = 250;
    int hoverLeaveTimeoutMSec = 250;

    qreal widgetWidth = 250;
    qreal collapserHeight = 40;
}

QnNotificationListWidget::QnNotificationListWidget(QGraphicsItem *parent, Qt::WindowFlags flags):
    base_type(parent, flags),
    m_hoverProcessor(new HoverFocusProcessor(this))
{
    registerAnimation(this);
    startListening();

    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->setHoverLeaveDelay(hoverLeaveTimeoutMSec);

    connect(this, SIGNAL(geometryChanged()), this, SLOT(at_geometry_changed()));
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

    if (which == Qt::MinimumSize || m_itemDataByItem.isEmpty()) {
        return QSizeF(widgetWidth, collapserHeight);
    }

    bool collapserRequired = false;
    QSizeF result(widgetWidth, 0);
    foreach (ItemData *data, m_itemDataByItem) {
        if (!data->isVisible()) {
            if (!collapserRequired)
                result.setHeight(result.height() + collapserHeight);
            collapserRequired = true;
            continue;
        }

        QSizeF itemSize = data->item->geometry().size();
        if (itemSize.isNull())
            continue;
        result.setHeight(result.height() + itemSize.height());
    }
    return result;
}

void QnNotificationListWidget::tick(int deltaMSecs) {

    // y-coord of the lowest item
    qreal bottomY = 0;

    bool canShowNew = true;
    foreach (QnNotificationItem* item, m_items) {
        ItemData* data = m_itemDataByItem[item];
        if (!data->isVisible())
            continue;

        bottomY = qMax(bottomY, item->geometry().bottom());
    }

    // updating state and animating
    QList<QnNotificationItem*> itemsToDelete;
    int counter = 0;
    foreach (QnNotificationItem* item, m_items) {
        counter++;

        ItemData* data = m_itemDataByItem[item];
        switch (data->state) {
        case ItemData::Collapsing: {
                qreal step = (qreal)deltaMSecs / (qreal)collapseTimeoutMs;
                data->animationValue = qMin(data->animationValue + step, data->targetValue);
                //item->setOpacity(value);
                QTransform transform = this->transform();
                transform.rotate(data->animationValue*(90), Qt::XAxis);
                item->setTransform(transform);

                if (qFuzzyCompare(data->animationValue, data->targetValue)) {
                    data->state = ItemData::Collapsed;
                    item->setVisible(false);
                }
                canShowNew = false; // maintaining order in the list
                break;
            }
        case ItemData::Collapsed: {
                if (canShowNew && (bottomY + item->geometry().height() <= geometry().height())) {
                    data->state = ItemData::Displaying;

                    item->setY(bottomY);
                    item->setX(0.0);//item->geometry().width());
                    item->setOpacity(1.0);
                    data->animationValue = 1.0;
                    data->targetValue = 0.0; //target x-coord

                    QTransform transform = this->transform();
                    transform.rotate(data->animationValue*(90), Qt::XAxis);
                    item->setTransform(transform);

                    item->setVisible(true);

                    updateGeometry();
                }
                canShowNew = false; // maintaining order in the list
                break;
            }
        case ItemData::Displaying: {
                qreal step = /*item->geometry().width() **/ (qreal)deltaMSecs / (qreal)popoutTimeoutMs;
                data->animationValue = qMax(data->animationValue - step, data->targetValue);
                //qreal value = qMax(item->x() - step, data->targetValue);
                //item->setX(value);

                QTransform transform = this->transform();
                transform.rotate(data->animationValue*(90), Qt::XAxis);
                item->setTransform(transform);

                if (qFuzzyCompare(data->animationValue, data->targetValue)) {
                    data->state = ItemData::Displayed;
                    data->targetValue = 0.0; //counter for display time
                }
                canShowNew = false;
                break;
            }
        case ItemData::Displayed: {
                if (m_hoverProcessor->isHovered() || data->locked)
                    break;

                qreal step = (qreal)deltaMSecs / (qreal)displayTimeoutMs;
                data->targetValue += step;
                if (data->targetValue >= 1.0) {
                    data->hide();
                }
                break;
            }
        case ItemData::Hiding: {
                qreal step = (qreal)deltaMSecs / (qreal)hideTimeoutMs;
                qreal value = qMax(item->opacity() - step, data->targetValue);
                item->setOpacity(value);
                if (qFuzzyCompare(value, data->targetValue)) {
                    data->state = ItemData::Hidden;
                    item->setVisible(false);
                }
                break;
            }
        case ItemData::Hidden: {
                //do not remove while iterating
                itemsToDelete << item;
                break;
            }
        }
    }

    // moving items up
    ItemData *previous = NULL;
    foreach (QnNotificationItem* item, m_items) {
        ItemData* data = m_itemDataByItem[item];
        if (!data->isVisible())
            continue;

        qreal targetY = (previous == NULL ? 0 : previous->item->geometry().bottom());
        qreal currentY = item->y();
        qreal stepY = currentY * (qreal) deltaMSecs / (qreal) moveUpTimeoutMs;
        if (currentY > targetY) {
            item->setY(qMax(currentY - stepY, targetY));
        }
        previous = data;

    }

    // remove unused items
    if (!itemsToDelete.isEmpty())
        updateGeometry();
    foreach(QnNotificationItem* item, itemsToDelete) {
        ItemData* data = m_itemDataByItem[item];
        delete data;

        disconnect(item, 0, this, 0);
        m_itemDataByItem.remove(item);
        m_items.removeOne(item);
        emit itemRemoved(item);
    }
}

void QnNotificationListWidget::addItem(QnNotificationItem *item, bool locked)  {
    item->setVisible(false);
    item->setMinimumWidth(widgetWidth);
    item->setMaximumWidth(widgetWidth);
    item->setClickableButtons(item->clickableButtons() | Qt::RightButton);
    connect(item, SIGNAL(clicked(Qt::MouseButton)), this, SLOT(at_item_clicked(Qt::MouseButton)));

    ItemData* data = new ItemData();
    data->item = item;
    data->state = ItemData::Collapsed;
    data->locked = locked;
    m_itemDataByItem.insert(item, data);
    m_items.append(item);
}

void QnNotificationListWidget::clear() {
    foreach (ItemData *data, m_itemDataByItem) {
        if (data->isVisible())
            data->hide();
        else
            data->state = ItemData::Hidden;
    }
}

void QnNotificationListWidget::at_item_clicked(Qt::MouseButton button) {
    if (button != Qt::RightButton)
        return;
    QnNotificationItem *item = dynamic_cast<QnNotificationItem *>(sender());
    if (!item)
        return;
    m_itemDataByItem[item]->unlockAndHide();
}

void QnNotificationListWidget::at_geometry_changed() {
    QLinkedList<QnNotificationItem*>::const_iterator i = m_items.constEnd();
    i--;
    for (; i != m_items.constEnd(); --i) {
        ItemData* data = m_itemDataByItem[*i];
        if (!data->isVisible())
            continue;
        if (data->state == ItemData::Hiding ||
                data->state == ItemData::Collapsing) //do not collapse already hiding item
            continue;

        if ((*i)->geometry().bottom() > geometry().bottom()) {
            data->state = ItemData::Collapsing;
            data->targetValue = 1.0;
            data->animationValue = 0.0;
        } else
            break;
    }

}


