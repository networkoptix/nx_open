#include "notification_list_widget.h"

#include <QtCore/QDateTime>

#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/processors/hover_processor.h>

namespace {

    int popoutTimeoutMs = 1000;
    int displayTimeoutMs = 5000;
    int hideTimeoutMs = 1000;
    int hoverLeaveTimeoutMSec = 250;

}

QnNotificationListWidget::QnNotificationListWidget(QGraphicsItem *parent, Qt::WindowFlags flags):
    base_type(parent, flags),
    m_hoverProcessor(new HoverFocusProcessor(this)),
    m_bottomY(0)
{
    registerAnimation(this);
    startListening();

    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->setHoverLeaveDelay(hoverLeaveTimeoutMSec);
}

QnNotificationListWidget::~QnNotificationListWidget() {
    while (m_items.isEmpty())
        delete m_items.takeFirst();
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
                if (isDisplaying == NULL && hasFreeSpaceY(state->item->geometry().height())) {
                    state->state = QnItemState::Displaying;
                    connect(state->item, SIGNAL(geometryChanged()), this, SLOT(at_item_geometryChanged()));
                    state->item->setY(m_bottomY);
                    m_bottomY = state->item->geometry().bottom();
                    state->item->setX(state->item->geometry().width());
                    state->item->setVisible(true);
                    state->item->setOpacity(1.0);
                    state->targetValue = 0.0;
                    isDisplaying = state;
                }
                break;
            }
        case QnItemState::Displaying: {
                qreal step = state->item->geometry().width() * (qreal)deltaMSecs / (qreal)popoutTimeoutMs;
                qreal value = qMax(state->item->x() - step, state->targetValue);
                state->item->setX(value);
                if (qFuzzyCompare(value, state->targetValue)) {
                    state->state = QnItemState::Displayed;
                    state->targetValue = 0.0;
                }
                break;
            }
        case QnItemState::Displayed: {
                qreal step = (qreal)deltaMSecs / (qreal)displayTimeoutMs;
                state->targetValue += step;
                if (state->targetValue >= 1.0 && !m_hoverProcessor->isHovered()) {
                    state->state = QnItemState::Hiding;
                    state->targetValue = 0.0;
                }
                break;
            }
        case QnItemState::Hiding: {
                qreal step = (qreal)deltaMSecs / (qreal)hideTimeoutMs;
                qreal value = qMax(state->item->opacity() - step, state->targetValue);
                state->item->setOpacity(value);
                if (qFuzzyCompare(value, state->targetValue)) {
                    state->state = QnItemState::Hidden;
                    state->item->setVisible(false);
                }
                break;
            }
        case QnItemState::Hidden: {
                state->state = QnItemState::Waiting;
                //here will be item removing from the list
                break;
            }
        }
    }

    bool anyVisible = false;
    foreach (QnItemState *state, m_items) {
        if (state->state == QnItemState::Displaying
                || state->state == QnItemState::Displayed
                || state->state == QnItemState::Hiding) {
            anyVisible = true;
            break;
        }
    }
    if (!anyVisible)
        m_bottomY = 0;

}

bool QnNotificationListWidget::hasFreeSpaceY(qreal required) const {
    return m_bottomY + required <= geometry().height();
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
