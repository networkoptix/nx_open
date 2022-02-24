// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QPointer>

#include <ui/graphics/items/controls/html_text_item.h>

#include "action_indicator_item.h"

class QGraphicsWidget;
class QnViewportBoundWidget;
class QnResourceTitleItem;
class QnHudOverlayWidget;

class QnHudOverlayWidgetPrivate: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_DECLARE_PUBLIC(QnHudOverlayWidget)
    QnHudOverlayWidget* const q_ptr;

public:
    QnHudOverlayWidgetPrivate(QnHudOverlayWidget* main);

    QnViewportBoundWidget* const titleHolder;
    QnResourceTitleItem* const title;
    QnViewportBoundWidget* const content;
    QnHtmlTextItem* const details;
    QnHtmlTextItem* const position;
    QGraphicsWidget* const left;
    QGraphicsWidget* const right;
    QnActionIndicatorItem* const actionIndicator;

private:
    void updateLayout();
};
