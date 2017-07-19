#pragma once

#include <QtCore/QHash>
#include <QtCore/QPointer>

#include <ui/customization/customized.h>

#include <ui/graphics/items/controls/html_text_item.h>

class QGraphicsWidget;
class QnViewportBoundWidget;
class QnResourceTitleItem;
class QnHudDetailsItem;
class QnHudPositionItem;
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
    QnHudDetailsItem* const details;
    QnHudPositionItem* const position;
    QGraphicsWidget* const left;
    QGraphicsWidget* const right;

private:
    void updateLayout();
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
