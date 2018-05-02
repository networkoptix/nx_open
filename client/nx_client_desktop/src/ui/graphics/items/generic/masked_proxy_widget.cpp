
#include "masked_proxy_widget.h"

#include <iostream>

#include <QtGui/QPainter>
#include <QtGui/QPaintEngine>
#include <QtWidgets/QStyleOptionGraphicsItem>
#include <QtWidgets/private/qgraphicsitem_p.h>
#include <QtWidgets/private/qwidget_p.h>

#include <nx/utils/math/fuzzy.h>
#include <ui/workaround/sharp_pixmap_painting.h>

namespace {

void blitPixmap(const QPixmap& source, QPixmap& destination, const QPoint& location = QPoint(0, 0))
{
    NX_ASSERT(source.devicePixelRatio() == destination.devicePixelRatio());

    QPainter blitter(&destination);
    blitter.setCompositionMode(QPainter::CompositionMode_Source);
    blitter.drawPixmap(location, source);
}

int sceneDevicePixelRatio(const QGraphicsScene* scene)
{
    int ratio = 1;
    if (!scene)
        return ratio;

    for (const auto view: scene->views())
        ratio = qMax(ratio, view->devicePixelRatio());

    return ratio;
}

} // namespace


QnMaskedProxyWidget::QnMaskedProxyWidget(QGraphicsItem* parent, Qt::WindowFlags windowFlags):
    QGraphicsProxyWidget(parent, windowFlags),
    m_updatesEnabled(true),
    m_pixmap(),
    m_itemCached(true),
    m_fullRepaintPending(false)
{
    /* In ItemCoordinateCache mode we cannot enforce sharp painting,
     * but can achieve accelerated scroll. Therefore it's better used for
     * big scrolling items with fixed position, like resource tree widget. */

    /* NoCache mode works only with patched Qt that ensures
     * full update of embedded widget when it's scrolled. */

    setCacheMode(ItemCoordinateCache);
}

QnMaskedProxyWidget::~QnMaskedProxyWidget()
{
    // There are some annoying bugs in graphics proxy widget implementation. Proxy widget in it's
    // base class destructor will call widget's destructor. After that some event filters and
    // slots may be called - but QGraphicsProxyWidget is already destroyed, what leads to crash.
    if (QWidget* w = widget())
    {
        w->disconnect(this);
        w->removeEventFilter(this);
    }
}

void QnMaskedProxyWidget::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* outputWidget)
{
    Q_UNUSED(outputWidget);

    if (!widget() || !widget()->isVisible())
        return;

    const bool itemCached = cacheMode() != NoCache;
    if (m_itemCached != itemCached)
    {
        m_itemCached = itemCached;
        m_fullRepaintPending = true;
        m_dirtyRect = QRect();
    }

    const QRect widgetRect = rect().toAlignedRect();
    QRect updateRect = widgetRect;

    if (!m_fullRepaintPending)
    {
        /* In cached modes we obtain dirty rects from paint requests.
         * We accumulate them in case updates are disabled: */
        if (m_itemCached)
            m_dirtyRect |= option->exposedRect.toAlignedRect();

        updateRect &= m_dirtyRect;
    }

    if (m_updatesEnabled)
    {
        const int devicePixelRatio = scene()
            ? sceneDevicePixelRatio(scene())
            : painter->device()->devicePixelRatio();

        renderWidgetRect(updateRect, devicePixelRatio);

        m_fullRepaintPending = false;
        m_dirtyRect = QRect();
    }

    if (m_pixmap.isNull())
        return;

    const QRect blitRect = m_itemCached
        ? updateRect
        : option->exposedRect.toAlignedRect();

    /* Rectangle to render, in logical coordinates: */
    const QRect renderRect = m_pixmapRect & (m_paintRect.isNull()
        ? blitRect
        : blitRect & m_paintRect.toAlignedRect());

    /* Source rectangle within pixmap, in device coordinates: */
    const QRect sourceRect {
        renderRect.topLeft() * m_pixmap.devicePixelRatio(),
        renderRect.size() * m_pixmap.devicePixelRatio() };

    if (cacheMode() != ItemCoordinateCache)
        paintPixmapSharp(painter, m_pixmap, renderRect, sourceRect);
    else
        painter->drawPixmap(renderRect, m_pixmap, sourceRect);
}

bool QnMaskedProxyWidget::ensurePixmap(const QSize& logicalSize, int devicePixelRatio)
{
    const QSize targetSize = logicalSize * devicePixelRatio;

    if (targetSize.width() > m_pixmap.size().width()
        || targetSize.height() > m_pixmap.size().height())
    {
        m_pixmap = QPixmap(targetSize);
        m_pixmap.setDevicePixelRatio(devicePixelRatio);
        m_pixmapRect = QRect(QPoint(), logicalSize);
        return true;
    }
    else if (m_pixmap.devicePixelRatio() != devicePixelRatio)
    {
        m_pixmap.setDevicePixelRatio(devicePixelRatio);
        return true;
    }

    return false;
}

void QnMaskedProxyWidget::renderWidgetRect(const QRect& logicalRect, int devicePixelRatio)
{
    const QRect widgetRect = rect().toAlignedRect();
    QRect updateRect = logicalRect;

    if (ensurePixmap(widgetRect.size(), devicePixelRatio))
        updateRect = widgetRect;

    if (!updateRect.isEmpty())
    {
        if (updateRect != widgetRect)
        {
            /* Paint into sub-cache: */
            QPixmap subPixmap(updateRect.size() * devicePixelRatio);
            subPixmap.setDevicePixelRatio(devicePixelRatio);
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

    if (m_updatesEnabled && !m_dirtyRect.isEmpty())
        update(m_dirtyRect);
}

bool QnMaskedProxyWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == widget())
    {
        switch (event->type())
        {
            case QEvent::UpdateRequest:
                if (cacheMode() != NoCache)
                    break;
                /* In not cached mode we obtain dirty rects from embedded widget updates: */
                syncDirtyRect();
                break;

            case QEvent::CursorChange:
                setCursor(widget()->cursor());
                break;

            default:
                break;
        }
    }

    return base_type::eventFilter(watched, event);
}

void QnMaskedProxyWidget::syncDirtyRect()
{
    if (m_fullRepaintPending || !widget())
        return;

    /*
    * IF YOU DO NOT RECEIVE EMBEDDED WIDGET UPDATES WHEN THE WIDGET IS VISIBLE,
    * it is most probably caused by it not having WA_Mapped attribute set due to Qt bug.
    * Try to avoid changing visibility of embedded widget or its children while
    * the proxy is invisible.
    */

    QWidgetPrivate::get(widget())->syncBackingStore();

    const auto d = QGraphicsItemPrivate::get(this);
    m_fullRepaintPending = d->fullUpdatePending;
    if (m_fullRepaintPending)
        m_dirtyRect = QRect();
    else
        m_dirtyRect |= d->needsRepaint.toAlignedRect();
}
