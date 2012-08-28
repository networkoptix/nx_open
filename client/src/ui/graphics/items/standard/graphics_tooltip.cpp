#include "graphics_tooltip.h"
#include "graphics_label.h"

class QnGraphicsTooltipLabel: public GraphicsLabel{
public:
    QnGraphicsTooltipLabel(const QString & text, QGraphicsItem *parent);
    ~QnGraphicsTooltipLabel();

    static QnGraphicsTooltipLabel *instance;
    bool eventFilter(QObject *, QEvent *);

    QBasicTimer hideTimer, expireTimer;

    bool fadingOut;

    void reuseTip(const QString &text);
    void hideTip();
    void hideTipImmediately();
    void setTipRect(QGraphicsItem *item, const QRect &r);
    void restartExpireTimer();
    bool tipChanged(const QPointF &pos, const QString &text, QGraphicsItem *item);
    void placeTip(const QPointF &pos, QGraphicsItem *item);
protected:
    void timerEvent(QTimerEvent *e);

    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
private:
    QGraphicsItem *item;
    QRect rect;
};

QnGraphicsTooltipLabel *QnGraphicsTooltipLabel::instance = 0;

QnGraphicsTooltipLabel::QnGraphicsTooltipLabel(const QString & text, QGraphicsItem *parent):
    GraphicsLabel(text, parent)
{
    delete instance;
    instance = this;
    setFlags(flags() | QGraphicsItem::ItemIgnoresTransformations);
    QPointF scenePos;
    setPos(scenePos);
    qDebug() << "scenebrect" << sceneBoundingRect();
    qApp->installEventFilter(this);
    setAcceptHoverEvents(true);
    fadingOut = false;
    reuseTip(text);
}

QnGraphicsTooltipLabel::~QnGraphicsTooltipLabel(){
    instance = 0;
}


void QnGraphicsTooltipLabel::restartExpireTimer()
{
    int time = 10000 + 40 * qMax(0, text().length()-100);
    expireTimer.start(time, this);
    hideTimer.stop();
}

void QnGraphicsTooltipLabel::reuseTip(const QString &text)
{
    setText(text);
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

void QnGraphicsTooltipLabel::setTipRect(QGraphicsItem *item, const QRect &r)
{
    /*if (!rect.isNull() && !w)
        qWarning("QToolTip::setTipRect: Cannot pass null widget if rect is set");
    else{
        widget = w;
        rect = r;
    }*/
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

void QnGraphicsTooltipLabel::placeTip(const QPointF &pos, QGraphicsItem *item)
{
/*    QRect screen = QApplication::desktop()->screenGeometry(getTipScreen(pos, w));
    QPoint p = pos;
    p += QPoint(2,
#ifdef Q_WS_WIN
                21
#else
                16
#endif
        );
    if (p.x() + this->width() > screen.x() + screen.width())
        p.rx() -= 4 + this->width();
    if (p.y() + this->height() > screen.y() + screen.height())
        p.ry() -= 24 + this->height();
    if (p.y() < screen.y())
        p.setY(screen.y());
    if (p.x() + this->width() > screen.x() + screen.width())
        p.setX(screen.x() + screen.width() - this->width());
    if (p.x() < screen.x())
        p.setX(screen.x());
    if (p.y() + this->height() > screen.y() + screen.height())
        p.setY(screen.y() + screen.height() - this->height());
    this->move(p);*/
}

bool QnGraphicsTooltipLabel::tipChanged(const QPointF &pos, const QString &text, QGraphicsItem *item)
{
    if (QnGraphicsTooltipLabel::instance->text() != text)
        return true;

  /*  if (o != widget)
        return true;

    if (!rect.isNull())
        return !rect.contains(pos);
    else*/
       return false;
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

void QnGraphicsTooltipLabel::hoverEnterEvent(QGraphicsSceneHoverEvent *event){
    qDebug() << "hover enter";
    GraphicsLabel::hoverEnterEvent(event);
}

void QnGraphicsTooltipLabel::hoverLeaveEvent(QGraphicsSceneHoverEvent *event){
    qDebug() << "hover leave";
    hideTip();
    GraphicsLabel::hoverLeaveEvent(event);
}

void GraphicsTooltip::showText(QString text, QGraphicsItem *item){
    QRect rect;
    QPointF pos = item ? item->sceneBoundingRect().bottomLeft(): QPointF();

    if (QnGraphicsTooltipLabel::instance && QnGraphicsTooltipLabel::instance->isVisible()){ // a tip does already exist
        if (text.isEmpty()){ // empty text means hide current tip
            QnGraphicsTooltipLabel::instance->hideTip();
            return;
        }
        else if (!QnGraphicsTooltipLabel::instance->fadingOut){
            // If the tip has changed, reuse the one
            // that is showing (removes flickering)
            QPointF localPos = pos;
            if (item)
                localPos = item->mapFromScene(pos);
            if (QnGraphicsTooltipLabel::instance->tipChanged(localPos, text, item)){
                QnGraphicsTooltipLabel::instance->reuseTip(text);
                QnGraphicsTooltipLabel::instance->setTipRect(item, rect);
                QnGraphicsTooltipLabel::instance->placeTip(pos, item);
            }
            return;
        }
    }

    if (!text.isEmpty()){ // no tip can be reused, create new tip:
        new QnGraphicsTooltipLabel(text, item); // sets QTipLabel::instance to itself
        QnGraphicsTooltipLabel::instance->setTipRect(item, rect);
        QnGraphicsTooltipLabel::instance->placeTip(pos, item);
        QnGraphicsTooltipLabel::instance->setObjectName(QLatin1String("graphics_tooltip_label"));
        QnGraphicsTooltipLabel::instance->show();
    }

}
