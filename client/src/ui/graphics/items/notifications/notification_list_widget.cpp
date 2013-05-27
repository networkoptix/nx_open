#include "notification_list_widget.h"

#include <QtCore/QDateTime>

#include <ui/graphics/items/notifications/notification_item.h>

namespace {

    int timeToMove = 2000;
    int timeToDisplay = 10000;
    int timeToHide = 2000;

}

QnNotificationListWidget::QnNotificationListWidget(QGraphicsItem *parent, Qt::WindowFlags flags):
    base_type(parent, flags),
    m_bottomY(0)
{
    registerAnimation(this);
    startListening();
}

QnNotificationListWidget::~QnNotificationListWidget() {
    foreach (QnItemState *state, m_items) {
        delete state;
    }
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
    foreach (QnItemState *state, m_items) {
        if (state->state == QnItemState::Waiting)
            continue;

        QSizeF itemSize = state->item->geometry().size();
        if (itemSize.isNull())
            continue;
        result.setWidth(qMax(result.width(), itemSize.width()));
        result.setHeight(result.height() + itemSize.height());
    }
    return result;
}

void QnNotificationListWidget::tick(int deltaMSecs) {
    QnItemState *isDisplaying = NULL;
    foreach (QnItemState *state, m_items) {
        if (state->state == QnItemState::Displaying) {
            isDisplaying = state;
            break;
        }
    }

    foreach (QnItemState *state, m_items) {
        switch (state->state) {
        case QnItemState::Waiting: {
                if (isDisplaying != NULL && isDisplaying != state /* || !isEnoughSpace() */)
                    break;

                state->state = QnItemState::Displaying;
                connect(state->item, SIGNAL(geometryChanged()), this, SLOT(at_item_geometryChanged()));
                state->item->setY(m_bottomY - state->item->geometry().height());
                state->item->setVisible(true);
                state->targetValue = m_bottomY;
                state->valueStep = state->item->geometry().height();
                isDisplaying = state;
                break;
            }
        case QnItemState::Displaying: {
                qreal step = state->valueStep * (qreal)deltaMSecs / (qreal)timeToMove;
                qreal value = qMin(state->item->y() + step, state->targetValue);
                state->item->setY(value);
                if (qFuzzyCompare(value, state->targetValue)) {
                    state->state = QnItemState::Displayed;
                    state->targetValue = 0.0;
                    m_bottomY = state->item->geometry().bottom();
                }
                break;
            }
        case QnItemState::Displayed: {
                qreal step = (qreal)deltaMSecs / (qreal)timeToDisplay;
                state->targetValue += step;
                if (state->targetValue >= 1.0) {
                    state->state = QnItemState::Hiding;
                    state->targetValue = 0.0;
                }
                break;
            }
        case QnItemState::Hiding: {
                qreal step = (qreal)deltaMSecs / (qreal)timeToHide;
                qreal value = qMax(state->item->opacity() - step, state->targetValue);
                state->item->setOpacity(value);
                if (qFuzzyCompare(value, state->targetValue)) {
                    state->state = QnItemState::Hidden;
                    state->item->setVisible(false);
                }
                break;
            }
        case QnItemState::Hidden: {
                //here will be item removing from the list
                break;
            }
        }
    }
}

void QnNotificationListWidget::addItem(QnNotificationItem *item)  {
    item->setText(QLatin1String("Motion detected on camera\nblablabla") + QString::number(m_items.size()));
    item->setZValue(-1* m_items.size());
    item->setVisible(false);

    QnItemState* state = new QnItemState();
    state->item = item;
    state->state = QnItemState::Waiting;
    m_items.append(state);
}

void QnNotificationListWidget::at_item_geometryChanged() {
    prepareGeometryChange();
    updateGeometry();
}
