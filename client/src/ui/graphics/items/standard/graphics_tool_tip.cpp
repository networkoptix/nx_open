#include "graphics_tool_tip.h"

#include <limits>

#include <QtCore/QBasicTimer>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtGui/QPainter>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QGraphicsScene>
#include <QtGui/QTextDocument>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QApplication>

#include <ui/animation/opacity_animator.h>
#include <ui/common/weak_graphics_item_pointer.h>
#include <ui/graphics/items/generic/proxy_label.h>

#include <utils/math/fuzzy.h>

namespace {
    const qreal toolTipMargin = 5.0;
}

// -------------------------------------------------------------------------- //
// GraphicsToolTipLabel
// -------------------------------------------------------------------------- //
class GraphicsToolTipLabel: public QnProxyLabel {
    typedef QnProxyLabel base_type;

public:
    GraphicsToolTipLabel(const QString &text, QGraphicsItem *newItem);
    virtual ~GraphicsToolTipLabel();

    static GraphicsToolTipLabel *instance;

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

    void reuseTip(const QString &newText, QGraphicsItem *newItem);
    void hideTip();
    void hideTipImmediately();
    void restartExpireTimer();
    void placeTip(const QPointF &pos, const QRectF &viewport);
    bool tipChanged(const QString &newText, QGraphicsItem *parent);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual void timerEvent(QTimerEvent *e) override;
    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) override;

private:
    QGraphicsItem *item() const {
        return m_item.data();
    }

private:
    QBasicTimer m_hideTimer, m_expireTimer;
    WeakGraphicsItemPointer m_item;
    QPointF m_oldParentPos;
};

GraphicsToolTipLabel *GraphicsToolTipLabel::instance = 0;

GraphicsToolTipLabel::GraphicsToolTipLabel(const QString &text, QGraphicsItem *newItem):
    base_type(text),
    m_item(0)
{
    delete instance;
    instance = this;
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    setPalette(QToolTip::palette());

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setXOffset(toolTipMargin);
    shadow->setYOffset(toolTipMargin);
    shadow->setBlurRadius(toolTipMargin * .5);
    setGraphicsEffect(shadow);

    qApp->installEventFilter(this);
    reuseTip(text, newItem);
}

GraphicsToolTipLabel::~GraphicsToolTipLabel() {
    instance = 0;
}

void GraphicsToolTipLabel::restartExpireTimer()
{
    int time = 10000 + 40 * qMax(0, text().length() - 100); /* Formula from the default tooltip. */
    m_expireTimer.start(time, this);
    m_hideTimer.stop();
}

void GraphicsToolTipLabel::reuseTip(const QString &newText, QGraphicsItem *newItem)
{
    if (item())
        item()->removeSceneEventFilter(this);

    if (scene() != newItem->scene()) {
        if (scene())
            scene()->removeItem(this);
        newItem->scene()->addItem(this);
    }

    m_item = newItem;
    m_oldParentPos = newItem->pos();

    setZValue(std::numeric_limits<qreal>::max());
    setText(newText);
    setMargin(toolTipMargin);
    resize(sizeHint(Qt::PreferredSize));
    newItem->installSceneEventFilter(this);
    newItem->setAcceptHoverEvents(true); /* This won't be undone. */
    restartExpireTimer();
}

void GraphicsToolTipLabel::hideTip()
{
    if (!m_hideTimer.isActive())
        m_hideTimer.start(300, this);
}

void GraphicsToolTipLabel::hideTipImmediately()
{
    opacityAnimator(this, 6.0)->animateTo(0.0);
}

bool GraphicsToolTipLabel::eventFilter(QObject *, QEvent *event)
{
    switch (event->type()) {
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

void GraphicsToolTipLabel::placeTip(const QPointF &pos, const QRectF &viewport)
{
    if(!item())
        return;

    QPointF p = pos;

    // default pos - below the button and below the cursor
    QRectF cursorRect = item()->sceneTransform().mapRect(QRectF(0, 0, 10, 20));

    // TODO: #Elric doesn't work well with tree tooltips
    //p.setY(qMax(p.y() + cursorRect.height(), item()->sceneBoundingRect().y() + item()->sceneBoundingRect().height()));
    p.setY(p.y() + cursorRect.height());

    QRectF self = item()->sceneTransform().mapRect(this->boundingRect());
    if (!item()->transform().isIdentity()){
        self = item()->transform().inverted().mapRect(self);
    }

    if (p.x() + self.width() > viewport.x() + viewport.width())
        p.setX(viewport.x() + viewport.width() - self.width());

    if (p.x() < viewport.x())
        p.setX(viewport.x());

    // if cursor is too high (o_O), place tip below the button
    if (p.y() < viewport.y())
        p.setY(item()->sceneBoundingRect().y() + item()->sceneBoundingRect().height());

    // if cursor is too low, place tip over the button
    if (p.y() + self.height() > viewport.y() + viewport.height())
        p.setY(item()->sceneBoundingRect().y() - self.height());

    this->setPos(p);
}

bool GraphicsToolTipLabel::tipChanged(const QString &newText, QGraphicsItem *parent) {
    return newText != this->text() || parent != this->item() || !qFuzzyEquals(parent->pos(), m_oldParentPos);
}

void GraphicsToolTipLabel::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QStyleOptionFrame opt;
    opt.rect = rect().toRect();

    style()->drawPrimitive(QStyle::PE_PanelTipLabel, &opt, painter);
    base_type::paint(painter, option, widget);
}

void GraphicsToolTipLabel::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_hideTimer.timerId() || event->timerId() == m_expireTimer.timerId()) {
        m_hideTimer.stop();
        m_expireTimer.stop();
        hideTipImmediately();
    }
}

bool GraphicsToolTipLabel::sceneEventFilter(QGraphicsItem *watched, QEvent *event) {
    if (event->type() == QEvent::GraphicsSceneHoverLeave && watched == item())
        hideTip();
    return base_type::sceneEventFilter(watched, event);
}


// -------------------------------------------------------------------------- //
// GraphicsToolTip
// -------------------------------------------------------------------------- //
void GraphicsToolTip::showText(const QString &text, QGraphicsView *view, QGraphicsItem *item, const QPoint &pos) {
    QPointF scenePos = view->mapToScene(pos);
    QWidget *viewport = view->childAt(pos);

    //window can be in the resize animation process, so no child will be found at current coordinates --gdm
    if (!viewport)
        return;

    QRectF sceneRect(
        view->mapToScene(viewport->geometry().topLeft()),
        view->mapToScene(viewport->geometry().bottomRight())
    );

    showText(text, item, scenePos, sceneRect);
}

void GraphicsToolTip::showText(const QString &text, QGraphicsItem *item, QPointF scenePos, QRectF sceneRect) {
    if (GraphicsToolTipLabel::instance && GraphicsToolTipLabel::instance->isVisible() && !qFuzzyEquals(GraphicsToolTipLabel::instance->opacity(), 0.0)) { 
        /* A tip does already exist. */
        if (text.isEmpty()) { 
            /* Empty text => hide current tip. */
            GraphicsToolTipLabel::instance->hideTip();
            return;
        } else {
            /* If the tip has changed, reuse the one that is being shown (removes flickering). */
            if (GraphicsToolTipLabel::instance->tipChanged(text, item)) {
                GraphicsToolTipLabel::instance->reuseTip(text, item);
                GraphicsToolTipLabel::instance->placeTip(scenePos, sceneRect);
            }
            opacityAnimator(GraphicsToolTipLabel::instance, 3.0)->animateTo(1.0);
            return;
        }
    }

    if (!text.isEmpty()) { /* No tip can be reused, create new tip. */
        new GraphicsToolTipLabel(text, item); /* Sets GraphicsToolTipLabel::instance to itself. */
        GraphicsToolTipLabel::instance->setOpacity(0.0);
        GraphicsToolTipLabel::instance->placeTip(scenePos, sceneRect);
        GraphicsToolTipLabel::instance->setObjectName(QLatin1String("graphics_tooltip_label"));

        opacityAnimator(GraphicsToolTipLabel::instance, 3.0)->animateTo(1.0);
    }
}
