#pragma once

class QnImageButtonWidget;
class QGraphicsItem;
class QnWorkbenchContext;
class QAction;

namespace NxUi {

QnImageButtonWidget* newActionButton(QGraphicsItem *parent, QnWorkbenchContext* context,
    QAction* action, int helpTopicId);
QnImageButtonWidget* newShowHideButton(QGraphicsItem* parent, QnWorkbenchContext* context,
    QAction* action);
QnImageButtonWidget* newPinButton(QGraphicsItem* parent, QnWorkbenchContext* context,
    QAction* action, bool smallIcon = false);

}