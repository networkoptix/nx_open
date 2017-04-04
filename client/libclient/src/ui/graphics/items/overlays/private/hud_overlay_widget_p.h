#pragma once

#include <QtCore/QHash>
#include <QtCore/QPointer>

#include <client/client_color_types.h>

class QGraphicsWidget;
class QnResourceTitleItem;
class QnHtmlTextItem;

class QnHudOverlayWidget;

class QnHudOverlayWidgetPrivate
{
    Q_DECLARE_PUBLIC(QnHudOverlayWidget)
    QnHudOverlayWidget* const q_ptr;

public:
    QnHudOverlayWidgetPrivate(QnHudOverlayWidget* main);

    void updateTextOptions();

    QnResourceTitleItem* const title;
    QnHtmlTextItem* const details;
    QnHtmlTextItem* const position;

    QGraphicsWidget* const left;
    QGraphicsWidget* const right;

    QnResourceHudColors colors;
};
