#pragma once

#include <ui/actions/actions.h>

class QnImageButtonWidget;
class QnBlinkingImageButtonWidget;
class QGraphicsItem;
class QnWorkbenchContext;

namespace NxUi {

QnImageButtonWidget* newActionButton(
    QGraphicsItem *parent,
    QnWorkbenchContext* context,
    QnActions::IDType actionId,
    int helpTopicId);

QnImageButtonWidget* newShowHideButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    QnActions::IDType actionId);

QnBlinkingImageButtonWidget* newBlinkingShowHideButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    QnActions::IDType actionId);

QnImageButtonWidget* newPinButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    QnActions::IDType actionId,
    bool smallIcon = false);

} //namespace NxUi
