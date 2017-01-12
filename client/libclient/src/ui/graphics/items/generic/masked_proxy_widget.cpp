
#include "masked_proxy_widget.h"

#include <iostream>

#include <QtGui/QPainter>
#include <QtGui/QPaintEngine>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <nx/utils/math/fuzzy.h>
#include <ui/common/geometry.h>
#include <ui/workaround/sharp_pixmap_painting.h>

namespace {

void blitPixmap(const QPixmap& source, QPixmap& destination, const QPoint& location = QPoint(0, 0))
{
    NX_ASSERT(source.devicePixelRatio() == destination.devicePixelRatio());

    QPainter blitter(&destination);
    blitter.setCompositionMode(QPainter::CompositionMode_Source);
    blitter.drawPixmap(location, source);
}

} // namespace


QnMaskedProxyWidget::QnMaskedProxyWidget(QGraphicsItem* parent, Qt::WindowFlags windowFlags):
    QGraphicsProxyWidget(parent, windowFlags),
    m_updatesEnabled(true),
    m_pixmap()
{
    /*
    * Caching must be enabled, otherwise full render of embedded widget occurs at each paint.
    */
    setCacheMode(ItemCoordinateCache);
}

QnMaskedProxyWidget::~QnMaskedProxyWidget()
{
}

void QnMaskedProxyWidget::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* outputWidget)
{
    Q_UNUSED(outputWidget);

    if (!widget() || !widget()->isVisible())
        return;

    const QRect widgetRect = rect().toAlignedRect();
    QRect updateRect = option->exposedRect.toAlignedRect() & widgetRect;

    if (m_updatesEnabled)
    {
        const int pixelRatio = painter->device()->devicePixelRatio();
        const QSize targetSize = widgetRect.size() * pixelRatio;

        if (m_pixmap.devicePixelRatio() != pixelRatio
            || targetSize.width() > m_pixmap.size().width()
            || targetSize.height() > m_pixmap.size().height())
        {
            m_pixmap = QPixmap(targetSize);
            m_pixmap.setDevicePixelRatio(pixelRatio);
            m_pixmapRect = widgetRect;
            updateRect = widgetRect;
        }

        if (!updateRect.isEmpty())
        {
            if (updateRect != widgetRect)
            {
                /* Paint into sub-cache: */
                QPixmap subPixmap(updateRect.size() * pixelRatio);
                subPixmap.setDevicePixelRatio(pixelRatio);
                subPixmap.fill(Qt::transparent);
                widget()->render(&subPixmap, QPoint(), updateRect);

                /* Blit sub-cache into main cache: */
                blitPixmap(subPixmap, m_pixmap, updateRect.topLeft());
            }
            else
            {
                /* Paint directly into cache: */
                m_pixmap.fill(Qt::transparent);
                widget()->render(&m_pixmap, updateRect.topLeft(), updateRect);
            }
        }
    }

    if (m_pixmap.isNull())
        return;

    /* Rectangle to render, in logical coordinates: */
    const QRect renderRect = m_pixmapRect & (m_paintRect.isNull()
        ? widgetRect
        : widgetRect & m_paintRect.toAlignedRect());

    /* Source rectangle within pixmap, in device coordinates: */
    const QRect sourceRect {
        renderRect.topLeft() * m_pixmap.devicePixelRatio(),
        renderRect.size() * m_pixmap.devicePixelRatio() };

    paintPixmapSharp(painter, m_pixmap, renderRect, sourceRect);
}

QRectF QnMaskedProxyWidget::paintRect() const
{
    return m_paintRect.isNull() ? rect() : m_paintRect;
}

void QnMaskedProxyWidget::setPaintRect(const QRectF& paintRect)
{
    if (qFuzzyEquals(m_paintRect, paintRect))
        return;

    const auto updateRegion = QRegion(paintRect.toAlignedRect())
        .xored(m_paintRect.toAlignedRect());

    for (const auto& rect: updateRegion.rects())
        update(rect);

    m_paintRect = paintRect;
    emit paintRectChanged();
}

QSizeF QnMaskedProxyWidget::paintSize() const
{
    return m_paintRect.isNull() ? size() : m_paintRect.size();
}

void QnMaskedProxyWidget::setPaintSize(const QSizeF& paintSize)
{
    setPaintRect(QRectF(m_paintRect.topLeft(), paintSize));
}

QRectF QnMaskedProxyWidget::paintGeometry() const
{
    if (m_paintRect.isNull())
        return geometry();

    return { pos() + m_paintRect.topLeft(), m_paintRect.size() };
}

void QnMaskedProxyWidget::setPaintGeometry(const QRectF& paintGeometry)
{
    setPaintRect({ paintGeometry.topLeft() - pos(), paintGeometry.size() });
}

bool QnMaskedProxyWidget::isUpdatesEnabled() const
{
    return m_updatesEnabled;
}

void QnMaskedProxyWidget::setUpdatesEnabled(bool updatesEnabled)
{
    if (m_updatesEnabled == updatesEnabled)
        return;

    m_updatesEnabled = updatesEnabled;

    if (m_updatesEnabled)
        update();
}
