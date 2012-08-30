#include "graphics_tooltip.h"
#include "graphics_label.h"

#include <limits>

#include <QtGui/QStyle>
#include <QtGui/QStyleOption>
#include <QtGui/QPainter>
#include <QtGui/QToolTip>
#include <QtGui/QGraphicsScene>

#define NOPARENT

class GraphicsTooltipLabel: public GraphicsLabel {

    typedef GraphicsLabel base_type;
public:
    GraphicsTooltipLabel(const QString & text, QGraphicsItem *newItem);
    ~GraphicsTooltipLabel();

    static GraphicsTooltipLabel *instance;
    bool eventFilter(QObject *, QEvent *);

    QBasicTimer hideTimer, expireTimer;
    void reuseTip(const QString &newText, QGraphicsItem *newItem);
    void hideTip();
    void hideTipImmediately();
    void restartExpireTimer();
    void placeTip(const QPointF &pos, const QRectF &viewport);
    bool tipChanged(const QString &newText, QGraphicsItem *parent);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
protected:
    void timerEvent(QTimerEvent *e);
    //virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) override;
private:
    QGraphicsItem *item;
};

GraphicsTooltipLabel *GraphicsTooltipLabel::instance = 0;

GraphicsTooltipLabel::GraphicsTooltipLabel(const QString & text, QGraphicsItem *newItem):
    base_type(text),
    item(0)
{
    delete instance;
    instance = this;
    setFlags(flags() | QGraphicsItem::ItemIgnoresTransformations);
    setPalette(QToolTip::palette());
    qApp->installEventFilter(this);
    reuseTip(text, newItem);
}

GraphicsTooltipLabel::~GraphicsTooltipLabel(){
    instance = 0;
}


void GraphicsTooltipLabel::restartExpireTimer()
{
    int time = 10000 + 40 * qMax(0, text().length()-100); //formula from the default tooltip
    expireTimer.start(time, this);
    hideTimer.stop();
}

void GraphicsTooltipLabel::reuseTip(const QString &newText, QGraphicsItem *newItem)
{
    if (item)
        item->removeSceneEventFilter(this);
#ifdef NOPARENT
    if (scene() != newItem->scene()){
        if (scene())
            scene()->removeItem(this);
        newItem->scene()->addItem(this);
    }
#else
    this->setParentItem(newItem);
#endif

    item = newItem;

    setZValue(std::numeric_limits<qreal>::max());
    setText(newText);
    resize(sizeHint(Qt::PreferredSize));
    newItem->installSceneEventFilter(this);
    newItem->setAcceptHoverEvents(true); // this wont be undone, can be stored in inner field
    restartExpireTimer();
}

void GraphicsTooltipLabel::hideTip()
{
    if (!hideTimer.isActive())
        hideTimer.start(300, this);
}

void GraphicsTooltipLabel::hideTipImmediately()
{
    close(); // to trigger QEvent::Close which stops the animation
    deleteLater();
}

bool GraphicsTooltipLabel::eventFilter(QObject *, QEvent *e)
{
    switch (e->type()) {
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::Wheel:
        hideTipImmediately();
        break;
    default:
        break;
    }
    return false;
}

void GraphicsTooltipLabel::placeTip(const QPointF &pos, const QRectF &viewport)
{
    QPointF p = pos;

    // default pos - below the button and below the cursor
    QRectF cursorRect = item->sceneTransform().mapRect(QRectF(0, 0, 10, 20));

    p.setY(qMax(p.y() + cursorRect.height(), item->sceneBoundingRect().y() + item->sceneBoundingRect().height()));

#ifdef NOPARENT
    QRectF self = item->sceneTransform().mapRect(this->boundingRect());
#else
    QRectF self = this->sceneBoundingRect();
#endif

    if (p.x() + self.width() > viewport.x() + viewport.width())
        p.setX(viewport.x() + viewport.width() - self.width());

    if (p.x() < viewport.x())
        p.setX(viewport.x());

    // if cursor is too high (o_O), place tip below the button
    if (p.y() < viewport.y())
        p.setY(item->sceneBoundingRect().y() + item->sceneBoundingRect().height());

    // if cursor is too low, place tip over the button
    if (p.y() + self.height() > viewport.y() + viewport.height())
        p.setY(item->sceneBoundingRect().y() - self.height());

#ifdef NOPARENT
    this->setPos(p);
#else
    this->setPos(item->mapFromScene(p));
#endif
}

bool GraphicsTooltipLabel::tipChanged(const QString &newText, QGraphicsItem *parent){
    return (newText != this->text() || parent != this->item);
}

void GraphicsTooltipLabel::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget){

    QStyleOptionFrame opt;
    opt.rect = this->boundingRect().toRect();
    opt.rect.adjust(-5, -5, 5, 5);

    style()->drawPrimitive(QStyle::PE_PanelTipLabel, &opt, painter);
    painter->setPen(QPen(Qt::black));
    base_type::paint(painter, option, widget);
}

void GraphicsTooltipLabel::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == hideTimer.timerId()
        || e->timerId() == expireTimer.timerId()){
        hideTimer.stop();
        expireTimer.stop();
        hideTipImmediately();
    }
}

bool GraphicsTooltipLabel::sceneEventFilter(QGraphicsItem *watched, QEvent *event){
    if (event->type() == QEvent::GraphicsSceneHoverLeave && watched == item){
        hideTip();
    }
    return base_type::sceneEventFilter(watched, event);
}

void GraphicsTooltip::showText(QString text, QGraphicsItem *item, QPointF pos, QRectF viewport){
    if (GraphicsTooltipLabel::instance && GraphicsTooltipLabel::instance->isVisible()){ // a tip does already exist
        if (text.isEmpty()){ // empty text means hide current tip
            GraphicsTooltipLabel::instance->hideTip();
            return;
        }
        else
        {
            // If the tip has changed, reuse the one
            // that is showing (removes flickering)
            if (GraphicsTooltipLabel::instance->tipChanged(text, item)){
                GraphicsTooltipLabel::instance->reuseTip(text, item);
                GraphicsTooltipLabel::instance->placeTip(pos, viewport);
            }
            return;
        }
    }

    if (!text.isEmpty()){ // no tip can be reused, create new tip:
        new GraphicsTooltipLabel(text, item); // sets GraphicsTooltipLabel::instance to itself
        GraphicsTooltipLabel::instance->placeTip(pos, viewport);
        GraphicsTooltipLabel::instance->setObjectName(QLatin1String("graphics_tooltip_label"));
        GraphicsTooltipLabel::instance->show();
    }

}
