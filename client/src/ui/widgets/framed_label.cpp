#include "framed_label.h"

#include <QtGui/QPainter>

#include <utils/common/scoped_painter_rollback.h>
#include "ui/common/palette.h"

QnFramedLabel::QnFramedLabel(QWidget *parent):
    base_type(parent),
    m_opacity(1.0)
{}

QnFramedLabel::~QnFramedLabel() {}

QSize QnFramedLabel::contentSize() const {
    return size() - QSize(lineWidth() * 2, lineWidth() * 2);
}

qreal QnFramedLabel::opacity() const {
    return m_opacity;
}

void QnFramedLabel::setOpacity(qreal value) {
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_opacity, value))
        return;

    m_opacity = value;
    update();
}

QColor QnFramedLabel::frameColor() const {
    return palette().color(QPalette::Highlight);
}

void QnFramedLabel::setFrameColor(const QColor color) {
    setPaletteColor(this, QPalette::Highlight, color);
}

void QnFramedLabel::paintEvent(QPaintEvent *event) {
    bool pixmapExists = pixmap() && !pixmap()->isNull();
    if (!pixmapExists) {
        base_type::paintEvent(event);
        return;
    }

    QPainter painter(this);
    QRect fullRect = rect().adjusted(lineWidth() / 2, lineWidth() / 2, -lineWidth() / 2, -lineWidth() / 2);

    if (pixmapExists) {
        QnScopedPainterOpacityRollback opacityRollback(&painter, painter.opacity() * m_opacity);
        QRect pix = pixmap()->rect();
        int x = fullRect.left() + (fullRect.width() - pix.width()) / 2;
        int y = fullRect.top() + (fullRect.height() - pix.height()) / 2;
        painter.drawPixmap(x, y, *pixmap());
    }

    if (lineWidth() == 0)
        return;

    QPen pen;
    pen.setWidth(lineWidth());
    pen.setColor(palette().color(QPalette::Highlight));
    QnScopedPainterPenRollback penRollback(&painter, pen);
    painter.drawRect(fullRect);
}

