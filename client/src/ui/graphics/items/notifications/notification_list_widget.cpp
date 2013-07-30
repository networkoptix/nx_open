#include "notification_list_widget.h"

#include <limits>

#include <boost/range/adaptor/reversed.hpp>

#include <QtCore/QDateTime>

#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/processors/hover_processor.h>

#include <ui/style/globals.h>

namespace {

    int popoutTimeoutMs = 300;
    int moveUpTimeoutMs = 500;
    int displayTimeoutMs = 12500;
    int hideTimeoutMs = 300;
    int collapseTimeoutMs = 250;
    int hoverLeaveTimeoutMSec = 250;

    qreal widgetWidth = 250;
    qreal collapserHeight = 30;
}

QnNotificationListWidget::QnNotificationListWidget(QGraphicsItem *parent, Qt::WindowFlags flags):
    base_type(parent, flags),
    m_hoverProcessor(new HoverFocusProcessor(this)),
    m_collapsedItemCountChanged(false),
    m_speedUp(1.0),
    m_itemNotificationLevel(Qn::OtherNotification)
{
    registerAnimation(this);
    startListening();

    setFlag(QGraphicsItem::ItemHasNoContents);

    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->setHoverLeaveDelay(hoverLeaveTimeoutMSec);

    m_collapser.item = new QnNotificationItem(this);
    m_collapser.item->setNotificationLevel(Qn::OtherNotification);
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
    m_itemDataByItem.clear();
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

    QSizeF result(widgetWidth, 0);
    // preferred height is height of the most bottom item + height of all hidden items
    foreach(QnNotificationItem *item, m_items | boost::adaptors::reversed) {
        ItemData* data = m_itemDataByItem[item];
        if (data->state == ItemData::Collapsed) {
            result.setHeight(result.height() + item->geometry().height());
        } else {
            result.setHeight(result.height() + item->geometry().bottom());
            break;
        }
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
    QList<QnNotificationItem *> itemsToDelete;
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
                    m_collapsedItemCountChanged = true;
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
                    data->setAnimation(item->geometry().width(), 0.0, popoutTimeoutMs / m_speedUp);
                    item->setX(data->animation.value);
                    item->setVisible(true);
                    m_collapsedItemCountChanged = true;
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
                    data->setAnimation(0.0, 1.0, displayTimeoutMs / m_speedUp);
                }
                canShowNew = false;
                break;
            }
        case ItemData::Displayed: {
                if (m_hoverProcessor->isHovered() || data->locked)
                    break;

                data->animationTick(deltaMSecs);
                if (data->animationFinished())
                    data->hide(m_speedUp);
                break;
            }
        case ItemData::Hiding: {
                data->animationTick(deltaMSecs);
                item->setOpacity(data->animation.value);
                if (data->animationFinished()) {
                    data->state = ItemData::Hidden;
                    item->setVisible(false);
                    m_collapsedItemCountChanged = true;
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

    if (m_collapsedItemCountChanged) {
        m_collapser.animation.source = m_collapser.item->y();
        m_collapser.item->setText(tr("%1 items more").arg(collapsedItemsCount));
        m_collapsedItemCountChanged = false;
        m_speedUp = 1.0 + collapsedItemsCount / 10.0;
    }

    // moving items up
    ItemData *previous = NULL;
    foreach (QnNotificationItem* item, m_items) {
        ItemData* data = m_itemDataByItem[item];
        if (!data->isVisible())
            continue;

        qreal targetY = (previous == NULL ? 0 : previous->item->geometry().bottom());
        qreal currentY = item->y();
        qreal stepY = qMax(currentY, item->geometry().height()) * (qreal) deltaMSecs / (moveUpTimeoutMs / m_speedUp);
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

    bool itemCountChange = !itemsToDelete.isEmpty();
    bool itemNotificationLevelChange = false;

    // remove unused items
    foreach(QnNotificationItem* item, itemsToDelete) {
        ItemData* data = m_itemDataByItem[item];
        delete data;

        int level = static_cast<int>(item->notificationLevel());
        if (level == m_itemNotificationLevel)
            itemNotificationLevelChange = true;

        disconnect(item, 0, this, 0);
        m_itemDataByItem.remove(item);
        m_items.removeOne(item);
        emit itemRemoved(item);
    }

    if (itemCountChange) {
        updateGeometry();
        emit itemCountChanged();
    }

    if (itemNotificationLevelChange && !m_items.isEmpty()) {
        QnNotificationItem *maxLevelItem = m_items.first();
        foreach (QnNotificationItem* item, m_items) {
            if (item->notificationLevel() > maxLevelItem->notificationLevel())
                maxLevelItem = item;
        }
        Qn::NotificationLevel level = maxLevelItem->notificationLevel();
        if (level != m_itemNotificationLevel) {
            m_itemNotificationLevel = level;
            emit notificationLevelChanged();
        }
    }

    updateVisibleSize();
}

void QnNotificationListWidget::updateGeometry() {
    base_type::updateGeometry();
    emit sizeHintChanged();
}

void QnNotificationListWidget::updateVisibleSize() {

    QSizeF size(widgetWidth, 0);
    if (!qFuzzyIsNull(m_collapser.item->opacity())) {
        size.setHeight(m_collapser.item->geometry().bottom());
    } else {
        foreach(QnNotificationItem *item, m_items | boost::adaptors::reversed) {
            ItemData* data = m_itemDataByItem[item];
            if (!data->isVisible())
                continue;
            size.setHeight(item->geometry().bottom());
            break;
        }
    }

    if (qFuzzyCompare(m_visibleSize, size))
        return;
    m_visibleSize = size;

    emit visibleSizeChanged();
    updateGeometry();
}

void QnNotificationListWidget::addItem(QnNotificationItem *item, bool locked)  {
    item->setVisible(false);
    item->setMinimumWidth(widgetWidth);
    item->setMaximumWidth(widgetWidth);
    item->setTooltipEnclosingRect(m_tooltipsEnclosingRect);
    connect(item, SIGNAL(closeTriggered()), this, SLOT(at_item_closeTriggered()));
    connect(item, SIGNAL(geometryChanged()), this, SLOT(at_item_geometryChanged()));

    ItemData* data = new ItemData();
    data->item = item;
    data->state = ItemData::Collapsed;
    data->locked = locked;
    data->cachedHeight = -1;
    m_itemDataByItem.insert(item, data);
    m_items.append(item);

    m_collapsedItemCountChanged = true;
    emit itemCountChanged();

    Qn::NotificationLevel level = item->notificationLevel();
    if (level > m_itemNotificationLevel) {
        m_itemNotificationLevel = level;
        emit notificationLevelChanged();
    }

    updateGeometry();
}

void QnNotificationListWidget::removeItem(QnNotificationItem *item) {
    ItemData* data = m_itemDataByItem[item];
    if (data->isVisible())
        data->hide(m_speedUp);
    else
        data->state = ItemData::Hidden;
    m_collapsedItemCountChanged = true;
}

void QnNotificationListWidget::clear() {
    foreach (QnNotificationItem* item, m_items) {
        ItemData* data = m_itemDataByItem[item];
        if (data->isVisible())
            data->hide(m_speedUp);
        else
            data->state = ItemData::Hidden;
    }

    m_collapsedItemCountChanged = true;
}

QSizeF QnNotificationListWidget::visibleSize() const {
    return m_visibleSize;
}

int QnNotificationListWidget::itemCount() const {
    return m_items.size();
}

Qn::NotificationLevel QnNotificationListWidget::notificationLevel() const {
    return m_itemNotificationLevel;
}

void QnNotificationListWidget::setToolTipsEnclosingRect(const QRectF &rect) {
    if (qFuzzyCompare(m_tooltipsEnclosingRect, rect))
        return;
    m_tooltipsEnclosingRect = rect;

    foreach(QnNotificationItem *item, m_items)
        item->setTooltipEnclosingRect(rect);
    m_collapser.item->setTooltipEnclosingRect(rect);
}

void QnNotificationListWidget::at_item_closeTriggered() {
    QnNotificationItem *item = dynamic_cast<QnNotificationItem *>(sender());
    if (!item)
        return;

    m_itemDataByItem[item]->unlockAndHide(m_speedUp);
}

void QnNotificationListWidget::at_item_geometryChanged() {
    QnNotificationItem *item = dynamic_cast<QnNotificationItem *>(sender());
    if (!item)
        return;
    ItemData* data = m_itemDataByItem[item];
    QRectF geom = item->geometry();
    if (geom.height() == data->cachedHeight)
        return;
    data->cachedHeight = geom.height();
    updateGeometry();
}

void QnNotificationListWidget::at_geometry_changed() {
    foreach(QnNotificationItem *item, m_items | boost::adaptors::reversed) {
        ItemData* data = m_itemDataByItem[item];
        if (!data->isVisible())
            continue;
        if (data->state == ItemData::Hiding ||
                data->state == ItemData::Collapsing) //do not collapse item tha is already hiding
            continue;

        if (item->geometry().bottom() > geometry().height()) {
            data->state = ItemData::Collapsing;
            data->setAnimation(0.0, 90.0, collapseTimeoutMs / m_speedUp);
        } else
            break;
    }
    updateVisibleSize();
}

void QnNotificationListWidget::ItemData::hide(qreal speedUp)  {
    if (state == Hiding)
        return;

    state = Hiding;
    setAnimation(1.0, 0.0, hideTimeoutMs / speedUp);
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

