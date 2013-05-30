#include "notification_list_widget.h"

#include <QtCore/QDateTime>

#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/processors/hover_processor.h>

#include <ui/style/globals.h>

namespace {

    int popoutTimeoutMs = 500;
    int moveUpTimeoutMs = 200;
    int displayTimeoutMs = 5000;
    int hideTimeoutMs = 300;
    int hoverLeaveTimeoutMSec = 250;

    qreal widgetWidth = 200;
}

QnNotificationListWidget::QnNotificationListWidget(QGraphicsItem *parent, Qt::WindowFlags flags):
    base_type(parent, flags),
    m_hoverProcessor(new HoverFocusProcessor(this)),
    m_counter(0)
{
    registerAnimation(this);
    startListening();

    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->setHoverLeaveDelay(hoverLeaveTimeoutMSec);

    connect(this, SIGNAL(geometryChanged()), this, SLOT(at_geometryChanged()));
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

    QSizeF result(widgetWidth, 0);
    foreach (QnItemState *state, m_items) {
        if (!state->isVisible())
            continue;

        QSizeF itemSize = state->item->geometry().size();
        if (itemSize.isNull())
            continue;
//        result.setWidth(qMax(result.width(), itemSize.width()));
        result.setHeight(result.height() + itemSize.height());
    }
    return result;
}

void QnNotificationListWidget::tick(int deltaMSecs) {

    qreal topY = m_items.isEmpty() ? 0 : m_items.first()->item->geometry().top();
    qreal stepY = (m_items.isEmpty() ? 0 : m_items.first()->item->geometry().height())
            * (qreal) deltaMSecs / (qreal) moveUpTimeoutMs;
    qreal bottomY = 0;

    //TODO: #GDM speed should depend on m_items.size (?)

    bool anyDisplaying = false;
    foreach (QnItemState *state, m_items) {
        if (!state->isVisible())
            continue;
        bottomY = qMax(bottomY, state->item->geometry().bottom());
        if (state->state == QnItemState::Displaying)
            anyDisplaying = true;
    }


    QnItemState *previous = NULL;
    QList<QnItemState*> itemsToDelete;

    foreach (QnItemState *state, m_items) {
        switch (state->state) {
        case QnItemState::Waiting: {
                if (!anyDisplaying && (bottomY + state->item->geometry().height() <= geometry().height())) {
                    anyDisplaying = true;

                    state->state = QnItemState::Displaying;

                    state->item->setMinimumWidth(widgetWidth);
                    state->item->setMaximumWidth(widgetWidth);
                    state->item->setY(bottomY);
                    state->item->setX(state->item->geometry().width());
                    state->item->setVisible(true);
                    state->item->setOpacity(1.0);
                    state->targetValue = 0.0; //target x-coord
                    state->item->setClickableButtons(state->item->clickableButtons() | Qt::RightButton);
                    connect(state->item, SIGNAL(clicked(Qt::MouseButton)), state, SLOT(unlockAndHide(Qt::MouseButton)));
                    updateGeometry();
                }
                break;
            }
        case QnItemState::Displaying: {
                qreal step = state->item->geometry().width() * (qreal)deltaMSecs / (qreal)popoutTimeoutMs;
                qreal value = qMax(state->item->x() - step, state->targetValue);
                state->item->setX(value);
                if (qFuzzyCompare(value, state->targetValue)) {
                    state->state = QnItemState::Displayed;
                    state->targetValue = 0.0; //counter for display time
                }
                break;
            }
        case QnItemState::Displayed: {
                if (m_hoverProcessor->isHovered() || state->locked)
                    break;

                qreal step = (qreal)deltaMSecs / (qreal)displayTimeoutMs;
                state->targetValue += step;
                if (state->targetValue >= 1.0)
                    state->hide();
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
                itemsToDelete << state;

                //here will be item removing from the list
                break;
            }
        }

        // moving items up
        if (state->isVisible()) {
            qreal targetY = (previous == NULL ? 0 : previous->item->geometry().bottom());
            qreal currentY = state->item->y();
            if (currentY > targetY)
                state->item->setY(qMax(currentY - stepY, targetY));
            previous = state;
        }
    }

    // remove unused items
    if (!itemsToDelete.isEmpty())
        updateGeometry();
    foreach(QnItemState* deleting, itemsToDelete) {
        disconnect(deleting->item, 0, this, 0);
        m_items.removeOne(deleting);
        emit itemRemoved(deleting->item);
        delete deleting;
    }
}

void QnNotificationListWidget::addItem(QnNotificationItem *item, bool locked)  {
    item->setVisible(false);

    QVector<QColor> colors = qnGlobals->statisticsColors().hdds;
    item->setColor(colors[m_counter % colors.size()]);

    QnItemState* state = new QnItemState(this);
    state->item = item;
    state->state = QnItemState::Waiting;
    state->locked = locked;
    m_items.append(state);
    m_counter++;
}

void QnNotificationListWidget::clear() {
    foreach (QnItemState *state, m_items) {
        if (state->isVisible())
            state->hide();
        else
            state->state = QnItemState::Hidden;
    }
}

void QnNotificationListWidget::at_geometryChanged() {
  /*  foreach (QnItemState *state, m_items) {
        if (!state->isVisible())
            continue;
        state->item->setMinimumWidth(geometry().width());
    }*/
}
