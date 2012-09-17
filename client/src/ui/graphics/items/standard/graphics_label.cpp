#include "graphics_label.h"
#include "graphics_label_p.h"

#include <QtGui/QFontMetricsF>

#include <utils/common/fuzzy.h>
#include <utils/common/scoped_painter_rollback.h>

void GraphicsLabelPrivate::init() {
    Q_Q(GraphicsLabel);

    performanceHint = GraphicsLabel::NoCaching;

    q->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred, QSizePolicy::Label));
}

void GraphicsLabelPrivate::updateCachedData() {
    Q_Q(GraphicsLabel);
    
    QRectF newRect;
    if (text.isEmpty()) {
        newRect = QRectF();
    } else if(performanceHint != GraphicsLabel::NoCaching) {
        staticText.setText(text);
        staticText.prepare(QTransform(), q->font());
        newRect = QRectF(QPointF(0.0, 0.0), staticText.size());
    } else {
        QFontMetricsF metrics(q->font());

        newRect = metrics.boundingRect(QRectF(0.0, 0.0, 0.0, 0.0), Qt::AlignCenter, text);
        newRect.moveTopLeft(QPointF(0.0, 0.0)); /* Italicized fonts may result in negative left border. */
    }
    
    if (!qFuzzyCompare(newRect, rect)) {
        rect = newRect;

        q->updateGeometry();
    }
}


GraphicsLabel::GraphicsLabel(QGraphicsItem *parent, Qt::WindowFlags f): 
    GraphicsFrame(*new GraphicsLabelPrivate, parent, f)
{
    Q_D(GraphicsLabel);
    d->init();
}

GraphicsLabel::GraphicsLabel(const QString &text, QGraphicsItem *parent, Qt::WindowFlags f): 
    GraphicsFrame(*new GraphicsLabelPrivate, parent, f)
{
    Q_D(GraphicsLabel);
    d->init();
    setText(text);
}

GraphicsLabel::~GraphicsLabel() {
    return;
}

QString GraphicsLabel::text() const {
    Q_D(const GraphicsLabel);

    return d->text;
}

void GraphicsLabel::setText(const QString &text) {
    Q_D(GraphicsLabel);
    if (d->text == text)
        return;

    d->text = text;
    d->updateCachedData();
}

void GraphicsLabel::clear() {
    setText(QString());
}

QStaticText::PerformanceHint GraphicsLabel::performanceHint() const { 
    return d_func()->performanceHint;
}

void GraphicsLabel::setPerformanceHint(QStaticText::PerformanceHint performanceHint) {
    Q_D(GraphicsLabel);

    d->performanceHint = performanceHint;
    if(performanceHint != NoCaching) {
        d->staticText.setPerformanceHint(performanceHint);
        d->updateCachedData();
    }
}

QSizeF GraphicsLabel::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    if(which == Qt::MinimumSize || which == Qt::PreferredSize)
        return d_func()->rect.size();

    return base_type::sizeHint(which, constraint);
}

void GraphicsLabel::changeEvent(QEvent *event) {
    Q_D(GraphicsLabel);

    switch(event->type()) {
    case QEvent::FontChange:
        d->updateCachedData();
        break;
    default:
        break;
    }

    base_type::changeEvent(event);
}

void GraphicsLabel::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_D(GraphicsLabel);

    base_type::paint(painter, option, widget);

    QnScopedPainterPenRollback penRollback(painter, palette().color(isEnabled() ? QPalette::Normal : QPalette::Disabled, QPalette::WindowText));
    QnScopedPainterFontRollback fontRollback(painter, font());

    if(d->performanceHint == NoCaching) {
        painter->drawText(rect(), Qt::AlignCenter, d->text);
    } else {
        painter->drawStaticText(0.0, 0.0, d->staticText);
    }
}

