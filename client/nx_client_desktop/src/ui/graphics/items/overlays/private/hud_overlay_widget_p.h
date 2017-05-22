#pragma once

#include <QtCore/QHash>
#include <QtCore/QPointer>

#include <ui/customization/customized.h>

#include <ui/graphics/items/controls/html_text_item.h>

class QGraphicsWidget;
class QnResourceTitleItem;
class QnHudDetailsItem;
class QnHudPositionItem;
class QnHudOverlayWidget;

class QnHudOverlayWidgetPrivate
{
    Q_DECLARE_PUBLIC(QnHudOverlayWidget)
    QnHudOverlayWidget* const q_ptr;

public:
    QnHudOverlayWidgetPrivate(QnHudOverlayWidget* main);

    QnResourceTitleItem* const title;
    QGraphicsWidget* const content;
    QnHudDetailsItem* const details;
    QnHudPositionItem* const position;
    QGraphicsWidget* const left;
    QGraphicsWidget* const right;
};

class QnHudDetailsItem: public Customized<QnHtmlTextItem> //< solely for possible customization
{
    Q_OBJECT
    using base_type = Customized<QnHtmlTextItem>;

public:
    using base_type::base_type; //< forward constructors
};

class QnHudPositionItem: public Customized<QnHtmlTextItem> //< solely for possible customization
{
    Q_OBJECT
    using base_type = Customized<QnHtmlTextItem>;

public:
    using base_type::base_type; //< forward constructors
};
