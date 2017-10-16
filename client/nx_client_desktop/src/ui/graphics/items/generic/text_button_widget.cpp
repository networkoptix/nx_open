#include "text_button_widget.h"
#include "image_button_widget_p.h"

#include <QtGui/QPainter>

#include <ui/common/palette.h>
#include <ui/common/text_pixmap_cache.h>
#include <nx/client/core/utils/geometry.h>

#include <utils/math/linear_combination.h>

using nx::client::core::utils::Geometry;

QnTextButtonWidget::QnTextButtonWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_alignment(Qt::AlignCenter),
    m_pixmapValid(false),
    m_relativeFrameWidth(-1.0)
{
    setFrameShape(Qn::NoFrame);
    setDynamic(true);
    std::fill(m_opacities.begin(), m_opacities.end(), -1.0);
    m_opacities[0] = 1.0;
}

const QString &QnTextButtonWidget::text() const {
    return m_text;
}

void QnTextButtonWidget::setText(const QString &text) {
    if(m_text == text)
        return;

    m_text = text;

    invalidatePixmap();
}

Qt::Alignment QnTextButtonWidget::alignment() const {
    return m_alignment;
}

void QnTextButtonWidget::setAlignment(Qt::Alignment alignment) {
    if(m_alignment == alignment)
        return;

    m_alignment = alignment;

    update();
}

QBrush QnTextButtonWidget::textBrush() const {
    return palette().brush(QPalette::WindowText);
}

void QnTextButtonWidget::setTextBrush(const QBrush &textBrush) {
    setPaletteBrush(this, QPalette::WindowText, textBrush);
}

QColor QnTextButtonWidget::textColor() const {
    return palette().color(QPalette::WindowText);
}

void QnTextButtonWidget::setTextColor(const QColor &textColor) {
    setPaletteColor(this, QPalette::WindowText, textColor);
}

qreal QnTextButtonWidget::relativeFrameWidth() const {
    return m_relativeFrameWidth;
}

void QnTextButtonWidget::setRelativeFrameWidth(qreal relativeFrameWidth) {
    if (qFuzzyEquals(m_relativeFrameWidth, relativeFrameWidth))
        return;

    m_relativeFrameWidth = relativeFrameWidth;
    update();
}

void QnTextButtonWidget::setGeometry(const QRectF &geometry) {
    if(m_relativeFrameWidth < 0) {
        base_type::setGeometry(geometry);
    } else {
        QSizeF oldSize = size();

        base_type::setGeometry(geometry);

        if(!qFuzzyEquals(oldSize, size()))
            setFrameWidth(qMin(size().height(), size().width()) * m_relativeFrameWidth);
    }
}

void QnTextButtonWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    ensurePixmap();

    /* Skip Framed implementation. */
    QnImageButtonWidget::paint(painter, option, widget);
}

void QnTextButtonWidget::paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget, const QRectF &rect) {
    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * linearCombine(1.0 - progress, stateOpacity(startState), progress, stateOpacity(endState)));

    /* Draw frame. */
    paintFrame(painter, rect);

    /* Draw image. */
    QnImageButtonWidget::paint(
        painter,
        startState,
        endState,
        progress,
        widget,
        Geometry::expanded(
            Geometry::aspectRatio(pixmap(0).size()),
            Geometry::eroded(rect, frameWidth()),
            Qt::KeepAspectRatio,
            m_alignment
        )
    );
}

void QnTextButtonWidget::changeEvent(QEvent *event) {
    base_type::changeEvent(event);

    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::FontChange:
        invalidatePixmap();
        break;
    default:
        break;
    }
}

void QnTextButtonWidget::invalidatePixmap() {
    m_pixmapValid = false;
}

void QnTextButtonWidget::ensurePixmap() {
    if(m_pixmapValid)
        return;

    if(m_text.isEmpty())
        return;

    setPixmap(0, QnTextPixmapCache::instance()->pixmap(m_text, font(), textColor()));
    m_pixmapValid = true;
}

QnTextButtonWidget::StateFlags QnTextButtonWidget::validOpacityState(StateFlags flags) const {
    return findValidState(flags, m_opacities, [](qreal opacity) { return opacity > 0; });
}

qreal QnTextButtonWidget::stateOpacity(StateFlags stateFlags) const {
    return m_opacities[validOpacityState(stateFlags)];
}

void QnTextButtonWidget::setStateOpacity(StateFlags stateFlags, qreal opacity) {
    m_opacities[stateFlags] = opacity;
}
