#include "graphics_label.h"
#include "graphics_label_p.h"

#include <QtGui/QFontMetricsF>

#include <utils/math/fuzzy.h>
#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/text_pixmap_cache.h>
#include <ui/common/geometry.h>
#include <ui/style/skin.h>

// -------------------------------------------------------------------------- //
// GraphicsLabelPrivate
// -------------------------------------------------------------------------- //
void GraphicsLabelPrivate::init() {
    Q_Q(GraphicsLabel);

    performanceHint = GraphicsLabel::NoCaching;
    alignment = Qt::AlignTop | Qt::AlignLeft;
    q->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred, QSizePolicy::Label));
}

void GraphicsLabelPrivate::updateSizeHint() {
    Q_Q(GraphicsLabel);
    
    QRectF newRect;
    if (text.isEmpty()) {
        newRect = QRectF();
        pixmap = QPixmap();
    } else {
        QFontMetricsF metrics(q->font());
        newRect = metrics.boundingRect(QRectF(0.0, 0.0, 0.0, 0.0), Qt::AlignCenter, text);
        newRect.moveTopLeft(QPointF(0.0, 0.0)); /* Italicized fonts may result in negative left border. */
    }
    
    if (!qFuzzyEquals(newRect, rect)) {
        rect = newRect;

        q->updateGeometry();
    }
}

void GraphicsLabelPrivate::ensurePixmaps() {
    Q_Q(GraphicsLabel);

    if(!pixmapDirty)
        return;

    pixmap = QnTextPixmapCache::instance()->pixmap(text, q->font(), q->palette().color(QPalette::WindowText));
    pixmapDirty = false;
}

void GraphicsLabelPrivate::ensureStaticText() {
    if(!staticTextDirty)
        return;

    staticText.setText(text);
    staticTextDirty = false;
}

QColor GraphicsLabelPrivate::textColor() const {
    Q_Q(const GraphicsLabel);

    return q->palette().color(q->isEnabled() ? QPalette::Normal : QPalette::Disabled, QPalette::WindowText);
}


// -------------------------------------------------------------------------- //
// GraphicsLabel
// -------------------------------------------------------------------------- //
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
    d->updateSizeHint();
    d->pixmapDirty = d->staticTextDirty = true;
    update();
}

void GraphicsLabel::clear() {
    setText(QString());
}

Qt::Alignment GraphicsLabel::alignment() const {
    return d_func()->alignment;
}

void GraphicsLabel::setAlignment(Qt::Alignment alignment) {
    Q_D(GraphicsLabel);

    if(d->alignment == alignment)
        return;

    d->alignment = alignment;

    update();
}

GraphicsLabel::PerformanceHint GraphicsLabel::performanceHint() const { 
    return d_func()->performanceHint;
}

void GraphicsLabel::setPerformanceHint(PerformanceHint performanceHint) {
    Q_D(GraphicsLabel);

    d->performanceHint = performanceHint;

    switch (performanceHint) {
    case ModerateCaching:
        d->staticText.setPerformanceHint(QStaticText::ModerateCaching);
        break;
    case AggressiveCaching:
        d->staticText.setPerformanceHint(QStaticText::AggressiveCaching);
        break;
    default:
        break;
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
        d->updateSizeHint();
        d->staticTextDirty = d->pixmapDirty = true;
        break;
    case QEvent::PaletteChange:
    case QEvent::EnabledChange:
        d->pixmapDirty = true;
        break;
    default:
        break;
    }

    base_type::changeEvent(event);
}

void GraphicsLabel::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_D(GraphicsLabel);

    base_type::paint(painter, option, widget);

    if(d->performanceHint == PixmapCaching) {
        d->ensurePixmaps();

        QPointF position = QnGeometry::aligned(d->pixmap.size(), rect(), d->alignment).topLeft();

        QTransform transform = painter->transform();

        /* Hand-rolled check for translation transform, 
         * with lower precision than standard QTransform::type. */
        qreal eps = 0.0001;
        if(
            qAbs(transform.m11() - 1.0) < eps &&
            qAbs(transform.m12()) < eps &&
            qAbs(transform.m21()) < eps &&
            qAbs(transform.m22() - 1.0) < eps &&
            qAbs(transform.m13()) < eps &&
            qAbs(transform.m23()) < eps &&
            qAbs(transform.m33() - 1.0) < eps
        ) {
            /* Instead of messing with QPainter::SmoothPixmapTransform, 
             * we simply adjust the transformation. */
            painter->setTransform(QTransform(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, qRound(transform.m31() + position.x()), qRound(transform.m32() + position.y()), 1.0));
            painter->drawPixmap(QPointF(), d->pixmap);
            painter->setTransform(transform);
        } else {
            painter->drawPixmap(position, d->pixmap);
        }
    } else {
        QnScopedPainterPenRollback penRollback(painter, d->textColor());
        QnScopedPainterFontRollback fontRollback(painter, font());

        if(d->performanceHint == NoCaching) {
            painter->drawText(rect(), d->alignment, d->text);
        } else {
            d->ensureStaticText();
            painter->drawStaticText(0.0, 0.0, d->staticText);
        }
    }
}

