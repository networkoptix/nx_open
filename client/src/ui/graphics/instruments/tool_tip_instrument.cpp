#include "tool_tip_instrument.h"

#include <QtGui/QHelpEvent>
#include <QtGui/QGraphicsSimpleTextItem>


class QnTooltipTextItem: public QGraphicsSimpleTextItem{

public:
    QnTooltipTextItem(const QString & text, QGraphicsItem * parent = 0):
        QGraphicsSimpleTextItem(text, parent){
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override{
        QGraphicsSimpleTextItem::paint(painter, option, widget);
        qDebug() << "paaaaa!";
    }
};

ToolTipInstrument::ToolTipInstrument(QObject *parent):
    Instrument(Viewport, makeSet(QEvent::ToolTip), parent)
{}

ToolTipInstrument::~ToolTipInstrument() {
    return;
}

bool ToolTipInstrument::event(QWidget *viewport, QEvent *event) {
    if(event->type() != QEvent::ToolTip)
        return base_type::event(viewport, event);

    QGraphicsView *view = this->view(viewport);
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
    QPointF scenePos = view->mapToScene(helpEvent->pos());

    QGraphicsItem *targetItem = NULL;
    foreach(QGraphicsItem *item, scene()->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder, view->viewportTransform())) {
        /* Note that we don't handle proxy widgets separately. */
        if (!item->toolTip().isEmpty()) {
            targetItem = item;
            break;
        }
    }

    if(targetItem) {
        qDebug() << "target item scene bounding" << targetItem->sceneBoundingRect();
        QFont f;
        f.setPointSizeF(20);
        QnTooltipTextItem *text = new QnTooltipTextItem(targetItem->toolTip(), targetItem);
      //  text->setFont(f);
        text->setPen(QPen(Qt::white));

        scene()->addItem(text);
        text->setFlags(text->flags() | QGraphicsItem::ItemIgnoresTransformations);
        text->setPos(0, 10);
        text->setVisible(true);
      //  scene()->update(text->sceneBoundingRect());
        qDebug() << "scenePos" << scenePos;
        qDebug() << "itempos" << text->sceneBoundingRect();


        // TODO: create tooltip item here.
    }

    helpEvent->setAccepted(targetItem != NULL);
    return true;
}

