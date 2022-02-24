// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/ui/actions/actions.h>

class QnImageButtonWidget;
class QnBlinkingImageButtonWidget;
class QGraphicsItem;
class QnWorkbenchContext;

namespace nx::vms::client::desktop {

QnImageButtonWidget* newActionButton(
    QGraphicsItem *parent,
    QnWorkbenchContext* context,
    nx::vms::client::desktop::ui::action::IDType actionId,
    int helpTopicId);

QnImageButtonWidget* newShowHideButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    nx::vms::client::desktop::ui::action::IDType actionId);

QnBlinkingImageButtonWidget* newBlinkingShowHideButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    nx::vms::client::desktop::ui::action::IDType actionId);

QnImageButtonWidget* newPinTimelineButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    nx::vms::client::desktop::ui::action::IDType actionId);

QnImageButtonWidget* newPinButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    nx::vms::client::desktop::ui::action::IDType actionId,
    bool smallIcon = false);

} //namespace nx::vms::client::desktop
