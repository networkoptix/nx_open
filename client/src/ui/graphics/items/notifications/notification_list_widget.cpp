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
    qreal collapserHeight = 30;
}

QnNotificationListWidget::QnNotificationListWidget(QGraphicsItem *parent, Qt::WindowFlags flags):
    base_type(parent, flags),
    m_hoverProcessor(new HoverFocusProcessor(this))
{
    registerAnimation(this);
    startListening();

    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->setHoverLeaveDelay(hoverLeaveTimeoutMSec);

    m_collapser.item = new QnNotificationItem(this);
    m_collapser.item->setColor(QColor(Qt::white));
    m_collapser.item->setText(tr("%1 items more").arg(0));
    m_collapser.item->setMinimumSize(QSizeF(widgetWidth, collapserHeight));
    m_collapser.item->setMaximumSize(QSizeF(widgetWidth, collapserHeight));
    m_collapser.item->setOpacity(0.0);
    m_collapser.item->setVisible(true);
    m_collapser.setAnimation(0, 0, moveUpTimeoutMs * 0.5);

    connect(this, SIGNAL(geometryChanged()), this, SLOT(at_geometry_changed()));
}

QnNotificationListWidget::~QnNotificationListWidget() {
    foreach(ItemData* data, m_itemDataByItem)
        delete data;
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
    foreach (QnNotificationItem* item, m_items) {
        ItemData* data = m_itemDataByItem[item];
        if (!data->isVisible()) {
            if (!collapserRequired)
                result.setHeight(result.height() + collapserHeight);
            collapserRequired = true;
            continue;
        }

        QSizeF itemSize = item->geometry().size();
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
    bool itemsCountChanged = false;
    int collapsedItemsCount = 0;

    foreach (QnNotificationItem* item, m_items) {
        ItemData* data = m_itemDataByItem[item];
        switch (data->state) {
        case ItemData::Collapsing: {
                data->animationTick(deltaMSecs);
                QTransform transform = this->transform();
                transform.rotate(data->animation.value, Qt::XAxis);
                item->setTransform(transform);

                if (data->animationFinished()) {
                    data->state = ItemData::Collapsed;
                    item->setVisible(false);
                    collapsedItemsCount++;
                    itemsCountChanged = true;
                }
                canShowNew = false; // maintaining order in the list
                break;
            }
        case ItemData::Collapsed: {
                if (canShowNew && (bottomY + item->geometry().height() <= geometry().height())) {
                    data->state = ItemData::Displaying;

                    item->setY(bottomY);
                    item->setOpacity(1.0);
                    item->setTransform(transform());
                    data->setAnimation(item->geometry().width(), 0.0, popoutTimeoutMs);
                    item->setX(data->animation.value);
                    item->setVisible(true);
                    itemsCountChanged = true;

                    updateGeometry();
                } else {
                    collapsedItemsCount++;
                }
                canShowNew = false; // maintaining order in the list
                break;
            }
        case ItemData::Displaying: {
                data->animationTick(deltaMSecs);
                item->setX(data->animation.value);

                if (data->animationFinished()) {
                    data->state = ItemData::Displayed;
                    data->setAnimation(0.0, 1.0, displayTimeoutMs);
                }
                canShowNew = false;
                break;
            }
        case ItemData::Displayed: {
                if (m_hoverProcessor->isHovered() || data->locked)
                    break;

                data->animationTick(deltaMSecs);
                if (data->animationFinished())
                    data->hide();
                break;
            }
        case ItemData::Hiding: {
                data->animationTick(deltaMSecs);
                item->setOpacity(data->animation.value);
                if (data->animationFinished()) {
                    data->state = ItemData::Hidden;
                    item->setVisible(false);
                    itemsCountChanged = true;
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

    if (itemsCountChanged) {
        m_collapser.animation.source = m_collapser.item->y();
        m_collapser.item->setText(tr("%1 items more").arg(collapsedItemsCount));
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

    // moving collapser
    {
        m_collapser.animation.target = (previous == NULL ? 0 : previous->item->geometry().bottom());
        m_collapser.animationTick(deltaMSecs);
        m_collapser.item->setY(m_collapser.animation.value);
        qreal opacityStep = (qreal)deltaMSecs / (qreal) hideTimeoutMs;
        if (collapsedItemsCount == 0)
            m_collapser.item->setOpacity(qMax(0.0, m_collapser.item->opacity() - opacityStep));
        else
            m_collapser.item->setOpacity(qMin(1.0, m_collapser.item->opacity() + opacityStep));
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
    foreach (QnNotificationItem* item, m_items) {
        ItemData* data = m_itemDataByItem[item];
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
    //reverse iterating
    QLinkedList<QnNotificationItem*>::const_iterator i = m_items.constEnd();
    i--;
    for (; i != m_items.constEnd(); --i) {
        ItemData* data = m_itemDataByItem[*i];
        if (!data->isVisible())
            continue;
        if (data->state == ItemData::Hiding ||
                data->state == ItemData::Collapsing) //do not collapse item tha is already hiding
            continue;

        if ((*i)->geometry().bottom() > geometry().height()) {
            data->state = ItemData::Collapsing;
            data->setAnimation(0.0, 90.0, collapseTimeoutMs);
        } else
            break;
    }
}

void QnNotificationListWidget::ItemData::hide()  {
    state = Hiding;
    setAnimation(1.0, 0.0, hideTimeoutMs);
}

void QnNotificationListWidget::ItemData::setAnimation(qreal from, qreal to, qreal time) {
    animation.source = from;
    animation.value = from;
    animation.target = to;
    animation.length = time;
}

void QnNotificationListWidget::ItemData::animationTick(qreal deltaMSecs) {
    qreal step = (animation.target - animation.source) * deltaMSecs / animation.length;
    if (animation.source < animation.target)
        animation.value = qMin(animation.value + step, animation.target);
    else
        animation.value = qMax(animation.value + step, animation.target);
}

bool QnNotificationListWidget::ItemData::animationFinished() {
    return qFuzzyCompare(animation.value, animation.target);
}

