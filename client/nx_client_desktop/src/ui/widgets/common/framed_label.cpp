#include "framed_label.h"

#include <QtGui/QPainter>

#include <utils/common/scoped_painter_rollback.h>
#include "ui/common/palette.h"

#include <nx/utils/math/fuzzy.h>

QnFramedLabel::QnFramedLabel(QWidget* parent):
    base_type(parent)
{
}

QnFramedLabel::~QnFramedLabel()
{
}

QSize QnFramedLabel::contentSize() const
{
    return size() - frameSize();
}

qreal QnFramedLabel::opacity() const
{
    return m_opacity;
}

void QnFramedLabel::setOpacity(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyEquals(m_opacity, value))
        return;

    m_opacity = value;
    update();
}

QColor QnFramedLabel::frameColor() const
{
    return palette().color(QPalette::Highlight);
}

void QnFramedLabel::setFrameColor(const QColor& color)
{
    setPaletteColor(this, QPalette::Highlight, color);
}

bool QnFramedLabel::autoScale() const
{
    return m_autoScale;
}

void QnFramedLabel::setAutoScale(bool value)
{
    if (m_autoScale == value)
        return;

    m_autoScale = value;
    updateGeometry();
}

QSize QnFramedLabel::sizeHint() const
{
    QSize sz = base_type::sizeHint();
    if (!pixmapExists())
        return sz;

    return sz + frameSize();
}

QSize QnFramedLabel::minimumSizeHint() const
{
    if (m_autoScale)
        return frameSize();

    QSize sz = base_type::minimumSizeHint();
    if (!pixmapExists())
        return sz;

    return sz + frameSize();
}

void QnFramedLabel::paintEvent(QPaintEvent* event)
{
    if (!pixmapExists())
    {
        base_type::paintEvent(event);
        return;
    }

    QPainter painter(this);
    QRect fullRect = rect().adjusted(
        lineWidth() / 2,
        lineWidth() / 2,
        -lineWidth() / 2,
        -lineWidth() / 2);

    // Opacity rollback scope.
    {
        QnScopedPainterOpacityRollback opacityRollback(&painter, painter.opacity() * m_opacity);

        QPixmap thumbnail = *pixmap();
        if (m_autoScale)
        {
            thumbnail = thumbnail.scaled(
                contentSize(),
                Qt::KeepAspectRatio,
                Qt::FastTransformation);
        }

        const auto pix = thumbnail.size();
        int x = fullRect.left() + (fullRect.width() - pix.width()) / 2;
        int y = fullRect.top() + (fullRect.height() - pix.height()) / 2;
        painter.drawPixmap(x, y, thumbnail);
    }

    if (lineWidth() == 0)
        return;

    QPen pen;
    pen.setWidth(lineWidth());
    pen.setColor(palette().color(QPalette::Highlight));
    QnScopedPainterPenRollback penRollback(&painter, pen);
    painter.drawRect(fullRect);
}

bool QnFramedLabel::pixmapExists() const
{
    return pixmap() && !pixmap()->isNull();
}

QSize QnFramedLabel::frameSize() const
{
    return QSize(lineWidth() * 2, lineWidth() * 2);
}

