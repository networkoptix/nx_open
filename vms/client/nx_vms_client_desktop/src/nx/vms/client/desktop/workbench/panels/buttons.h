// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/menu/actions.h>

class QnImageButtonWidget;
class QnBlinkingImageButtonWidget;
class QGraphicsItem;

namespace nx::vms::client::desktop {

class WindowContext;

QnImageButtonWidget* newActionButton(
    QGraphicsItem *parent,
    WindowContext* context,
    menu::IDType actionId,
    int helpTopicId);

QnImageButtonWidget* newShowHideButton(
    QGraphicsItem* parent,
    WindowContext* context,
    menu::IDType actionId);

QnBlinkingImageButtonWidget* newBlinkingShowHideButton(
    QGraphicsItem* parent,
    WindowContext* context,
    menu::IDType actionId);

QnImageButtonWidget* newPinTimelineButton(
    QGraphicsItem* parent,
    WindowContext* context,
    menu::IDType actionId);

} //namespace nx::vms::client::desktop
