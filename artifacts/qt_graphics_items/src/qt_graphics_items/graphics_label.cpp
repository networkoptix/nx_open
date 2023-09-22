// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 1992-2005 Trolltech AS. All rights reserved.
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "graphics_label.h"

#include <QtGui/QFontMetricsF>
#include <QtGui/QGuiApplication>

#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <ui/common/text_pixmap_cache.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/scoped_painter_rollback.h>

#include "graphics_label_p.h"

using nx::vms::client::core::Geometry;

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

    QSizeF newPreferredSize;
    QSizeF newMinimumSize;
    if (text.isEmpty()) {
        newPreferredSize = QSizeF();
        pixmap = QPixmap();
    } else {
        QFontMetricsF metrics(q->font());

        auto newPreferredRect = metrics.boundingRect(QRectF(0.0, 0.0, 0.0, 0.0), Qt::AlignCenter, text);
        newPreferredRect.moveTopLeft(QPointF(0.0, 0.0)); /* Italicized fonts may result in negative left border. */
        newPreferredSize = newPreferredRect.size();

        constexpr auto kMinimumText = "...";
        auto newMinimumRect = metrics.boundingRect(QRectF(0.0, 0.0, 0.0, 0.0), Qt::AlignCenter, kMinimumText);
        newMinimumRect.moveTopLeft(QPointF(0.0, 0.0)); /* Italicized fonts may result in negative left border. */
        newMinimumSize = newMinimumRect.size();
    }

    if (!qFuzzyEquals(newPreferredSize, preferredSize) || !qFuzzyEquals(newMinimumSize, minimumSize)) {
        preferredSize = newPreferredSize;
        minimumSize = newMinimumSize;
        q->updateGeometry();
    }
}

void GraphicsLabelPrivate::ensurePixmaps()
{
    Q_Q(GraphicsLabel);

    if(!pixmapDirty)
        return;

    pixmap = QnTextPixmapCache::instance()->pixmap(
        calculateText(),
        q->font(),
        q->palette().color(QPalette::WindowText),
        qApp->devicePixelRatio(),
        shadowRadius);
    pixmapDirty = false;
}

void GraphicsLabelPrivate::ensureStaticText() {
    if(!staticTextDirty)
        return;

    staticText.setText(calculateText());
    staticTextDirty = false;
}

QColor GraphicsLabelPrivate::textColor() const {
    Q_Q(const GraphicsLabel);

    return q->palette().color(q->isEnabled() ? QPalette::Normal : QPalette::Disabled, QPalette::WindowText);
}

QString GraphicsLabelPrivate::calculateText() const
{
    if (elideMode == Qt::ElideNone)
        return text;

    QFontMetricsF metrics(q_func()->font());
    return metrics.elidedText(
        text,
        elideMode,
        std::max(q_func()->size().width(), static_cast<qreal>(elideConstraint)),
        Qt::AlignCenter);
}

// -------------------------------------------------------------------------- //
// GraphicsLabel
// -------------------------------------------------------------------------- //
GraphicsLabel::GraphicsLabel(QGraphicsItem* parent, Qt::WindowFlags flags):
    GraphicsFrame(*new GraphicsLabelPrivate, parent, flags)
{
    Q_D(GraphicsLabel);
    d->init();
}

GraphicsLabel::GraphicsLabel(const QString& text, QGraphicsItem* parent, Qt::WindowFlags flags):
    GraphicsFrame(*new GraphicsLabelPrivate, parent, flags)
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

void GraphicsLabel::setShadowRadius(qreal radius)
{
    Q_D(GraphicsLabel);

    d->shadowRadius = radius;
    d->pixmapDirty = true;
    update();
}

Qt::TextElideMode GraphicsLabel::elideMode() const
{
    return d_func()->elideMode;
}

void GraphicsLabel::setElideMode(Qt::TextElideMode elideMode)
{
    Q_D(GraphicsLabel);

    if (d->elideMode == elideMode)
        return;

    d->elideMode = elideMode;
    d->pixmapDirty = true;
    d->staticTextDirty = true;
    d->updateSizeHint();
    setAcceptHoverEvents(d->elideMode != Qt::ElideNone);
    update();
}

int GraphicsLabel::elideConstraint() const
{
    return d_func()->elideConstraint;
}

void GraphicsLabel::setElideConstraint(int constraint)
{
    Q_D(GraphicsLabel);

    if (d->elideConstraint == constraint)
        return;

    d->elideConstraint = constraint;
    if (d->elideMode == Qt::ElideNone)
        return;

    d->pixmapDirty = true;
    d->staticTextDirty = true;
    update();
}

QSizeF GraphicsLabel::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    const auto impl = d_func();

    if (which == Qt::MinimumSize)
        return impl->elideMode == Qt::ElideNone ? impl->preferredSize : impl->minimumSize;

    if (which == Qt::PreferredSize)
        return impl->preferredSize;

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

void GraphicsLabel::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    base_type::hoverEnterEvent(event);

    if (d_func()->elideMode != Qt::ElideNone)
        setToolTip(d_func()->text);
}

void GraphicsLabel::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    base_type::hoverLeaveEvent(event);

    if (d_func()->elideMode != Qt::ElideNone)
        setToolTip(QString{});
}

void GraphicsLabel::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_D(GraphicsLabel);
    base_type::paint(painter, option, widget);

    if (d->elideMode != Qt::ElideNone)
    {
        const auto currentSize = rect().size();
        if (!qFuzzyEquals(d->cachedSize, currentSize))
        {
            d->cachedSize = currentSize;
            d->staticTextDirty = d->pixmapDirty = true;
        }
    }

    if (d->performanceHint == PixmapCaching)
    {
        d->ensurePixmaps();
        const auto dpiPixmapSize = (d->pixmap.size() / d->pixmap.devicePixelRatio());
        const QPointF position = Geometry::aligned(dpiPixmapSize, rect(), d->alignment).topLeft();
        paintPixmapSharp(painter, d->pixmap, position);
    }
    else
    {
        QnScopedPainterPenRollback penRollback(painter, d->textColor());
        QnScopedPainterFontRollback fontRollback(painter, font());

        if (d->performanceHint == NoCaching)
        {
            painter->drawText(rect(), d->alignment, d->calculateText());
        }
        else
        {
            d->ensureStaticText();
            const QPointF position = Geometry::aligned(d->staticText.size(), rect(), d->alignment).topLeft();
            painter->drawStaticText(position, d->staticText);
        }
    }
}
