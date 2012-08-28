#include "graphics_tooltip.h"
#include "graphics_label.h"

#include <QtGui/QStyle>
#include <QtGui/QStyleOption>
#include <QtGui/QPainter>
#include <QtGui/QToolTip>

class QnGraphicsTooltipLabel: public GraphicsLabel{

    typedef GraphicsLabel base_type;
public:
    QnGraphicsTooltipLabel(const QString & text, QGraphicsItem *parent);
    ~QnGraphicsTooltipLabel();

    static QnGraphicsTooltipLabel *instance;
    bool eventFilter(QObject *, QEvent *);

    QBasicTimer hideTimer, expireTimer;
    void reuseTip(const QString &text);
    void hideTip();
    void hideTipImmediately();
    void restartExpireTimer();
    void placeTip(const QPointF &pos, const QRectF &viewport);
    bool tipChanged(const QString &text, QGraphicsItem *item);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
protected:
    void timerEvent(QTimerEvent *e);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
private:
    QGraphicsItem *item;
};

QnGraphicsTooltipLabel *QnGraphicsTooltipLabel::instance = 0;

QnGraphicsTooltipLabel::QnGraphicsTooltipLabel(const QString & text, QGraphicsItem *parent):
    base_type(text, parent),
    item(parent)
{
    delete instance;
    instance = this;
    setFlags(flags() | QGraphicsItem::ItemIgnoresTransformations);
    //setPerformanceHint(QStaticText::AggressiveCaching);
  //  setFrameShape(GraphicsFrame::Box);

    QPointF scenePos;
    setPos(scenePos);
    setPalette(QToolTip::palette());
    qApp->installEventFilter(this);
    setAcceptHoverEvents(true);
    reuseTip(text);
}

QnGraphicsTooltipLabel::~QnGraphicsTooltipLabel(){
    instance = 0;
}


void QnGraphicsTooltipLabel::restartExpireTimer()
{
    int time = 10000 + 40 * qMax(0, text().length()-100); //formula from the default tooltip
    expireTimer.start(time, this);
    hideTimer.stop();
}

void QnGraphicsTooltipLabel::reuseTip(const QString &text)
{
    setText(text);
    resize(sizeHint(Qt::PreferredSize));
    restartExpireTimer();
}

void QnGraphicsTooltipLabel::hideTip()
{
    if (!hideTimer.isActive())
        hideTimer.start(300, this);
}

void QnGraphicsTooltipLabel::hideTipImmediately()
{
    close(); // to trigger QEvent::Close which stops the animation
    deleteLater();
}

bool QnGraphicsTooltipLabel::eventFilter(QObject *, QEvent *e)
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

void QnGraphicsTooltipLabel::placeTip(const QPointF &pos, const QRectF &viewport)
{
    // ensure that mouse cursor is inside rect
    QPointF p = pos - QPointF(10.0, 10.0);

    QRectF self = this->sceneBoundingRect();
    if (p.y() < viewport.y())
        p.setY(viewport.y());
    if (p.x() + self.width() > viewport.x() + viewport.width())
        p.setX(viewport.x() + viewport.width() - self.width());
    if (p.x() < viewport.x())
        p.setX(viewport.x());
    if (p.y() + self.height() > viewport.y() + viewport.height())
        p.setY(viewport.y() + viewport.height() - self.height());
    this->setPos(this->item->mapFromScene(p));
}

bool QnGraphicsTooltipLabel::tipChanged(const QString &text, QGraphicsItem *item){
    return (text != this->text() ||
            item != this->item);
}

void QnGraphicsTooltipLabel::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget){

    QStyleOptionFrame opt;
    opt.rect = this->boundingRect().toRect();
    opt.rect.adjust(-5, -5, 5, 5);

    style()->drawPrimitive(QStyle::PE_PanelTipLabel, &opt, painter);
    painter->setPen(QPen(Qt::black));
    base_type::paint(painter, option, widget);
}

void QnGraphicsTooltipLabel::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == hideTimer.timerId()
        || e->timerId() == expireTimer.timerId()){
        hideTimer.stop();
        expireTimer.stop();
        hideTipImmediately();
    }
}

void QnGraphicsTooltipLabel::hoverLeaveEvent(QGraphicsSceneHoverEvent *event){
    hideTip();
    base_type::hoverLeaveEvent(event);
}

void GraphicsTooltip::showText(QString text, QGraphicsItem *item, QPointF pos, QRectF viewport){
    if (QnGraphicsTooltipLabel::instance && QnGraphicsTooltipLabel::instance->isVisible()){ // a tip does already exist
        if (text.isEmpty()){ // empty text means hide current tip
            QnGraphicsTooltipLabel::instance->hideTip();
            return;
        }
        else
        {
            // If the tip has changed, reuse the one
            // that is showing (removes flickering)
            if (QnGraphicsTooltipLabel::instance->tipChanged(text, item)){
                QnGraphicsTooltipLabel::instance->reuseTip(text);
                QnGraphicsTooltipLabel::instance->placeTip(pos, viewport);
            }
            return;
        }
    }

    if (!text.isEmpty()){ // no tip can be reused, create new tip:
        new QnGraphicsTooltipLabel(text, item); // sets QnGraphicsTooltipLabel::instance to itself
        QnGraphicsTooltipLabel::instance->placeTip(pos, viewport);
        QnGraphicsTooltipLabel::instance->setObjectName(QLatin1String("graphics_tooltip_label"));
        QnGraphicsTooltipLabel::instance->show();
    }

}
