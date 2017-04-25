#pragma once

#include <nx/client/desktop/ui/actions/actions.h>

class QnImageButtonWidget;
class QnBlinkingImageButtonWidget;
class QGraphicsItem;
class QnWorkbenchContext;

namespace NxUi {

QnImageButtonWidget* newActionButton(
    QGraphicsItem *parent,
    QnWorkbenchContext* context,
    nx::client::desktop::ui::action::IDType actionId,
    int helpTopicId);

QnImageButtonWidget* newShowHideButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    nx::client::desktop::ui::action::IDType actionId);

QnBlinkingImageButtonWidget* newBlinkingShowHideButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    nx::client::desktop::ui::action::IDType actionId);

QnImageButtonWidget* newPinButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    nx::client::desktop::ui::action::IDType actionId,
    bool smallIcon = false);

} //namespace NxUi
