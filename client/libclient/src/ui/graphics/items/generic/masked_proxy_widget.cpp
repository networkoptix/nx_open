
#include "masked_proxy_widget.h"

#include <iostream>

#include <QtGui/QPainter>
#include <QtGui/QPaintEngine>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <nx/utils/math/fuzzy.h>

#include <ui/common/geometry.h>

QnMaskedProxyWidget::QnMaskedProxyWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    QGraphicsProxyWidget(parent, windowFlags),
    m_pixmapDirty(true),
    m_updatesEnabled(true),
    m_pixmap(),
    m_opaqueDrag(false)
{
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

    /* Filter out repaints on the window frame. */
    QRectF renderRectF = (option->exposedRect.isEmpty() ?
        rect() : option->exposedRect & rect());

    /* Filter out areas that are masked out. */
    if (!m_paintRect.isNull())
        renderRectF = renderRectF & m_paintRect;

    /* Nothing to paint? Just return. */
    QRect renderRect = renderRectF.toAlignedRect();
    if (renderRect.isEmpty())
        return;

    if (m_pixmapDirty && m_updatesEnabled)
    {
        const auto widgetRect = this->widget()->rect();
        const int ratio = painter->device()->devicePixelRatio();
        m_pixmap = QPixmap(widgetRect.size() * ratio);
        m_pixmap.fill(Qt::transparent);
        m_pixmap.setDevicePixelRatio(ratio);
        widget()->render(&m_pixmap, QPoint(), widgetRect);

        m_pixmapDirty = false;
    }
    const auto aspect = m_pixmap.devicePixelRatio();
    const auto size = renderRect.size() * aspect;
    const auto topLeft = (renderRect.topLeft() - rect().topLeft()) * aspect;
    painter->drawPixmap(renderRect, m_pixmap, QRectF(topLeft, size));
}

bool QnMaskedProxyWidget::eventFilter(QObject* object, QEvent* event)
{
    if (object == widget())
    {
        if (event->type() == QEvent::UpdateRequest)
            m_pixmapDirty = true;

#ifdef Q_OS_MAC
        if (event->type() != QEvent::Paint)
            m_pixmapDirty = true;
#endif
    }

    return base_type::eventFilter(object, event);
}

void QnMaskedProxyWidget::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    if (!m_opaqueDrag)
        base_type::dragEnterEvent(event);
}

void QnMaskedProxyWidget::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
    if (!m_opaqueDrag)
        base_type::dragLeaveEvent(event);
}

void QnMaskedProxyWidget::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
    if (!m_opaqueDrag)
        base_type::dragMoveEvent(event);
}

void QnMaskedProxyWidget::dropEvent(QGraphicsSceneDragDropEvent* event)
{
    if (!m_opaqueDrag)
        base_type::dropEvent(event);
}

QRectF QnMaskedProxyWidget::paintRect() const
{
    return m_paintRect.isNull() ? rect() : m_paintRect;
}

void QnMaskedProxyWidget::setPaintRect(const QRectF& paintRect)
{
    if (qFuzzyEquals(m_paintRect, paintRect))
        return;

    m_paintRect = paintRect;
    update();

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

    return QRectF(
        pos() + m_paintRect.topLeft(),
        m_paintRect.size()
    );
}

void QnMaskedProxyWidget::setPaintGeometry(const QRectF& paintGeometry)
{
    setPaintRect(QRectF(
        paintGeometry.topLeft() - pos(),
        paintGeometry.size()
    ));
}

bool QnMaskedProxyWidget::opaqueDrag() const
{
    return m_opaqueDrag;
}

void QnMaskedProxyWidget::setOpaqueDrag(bool value)
{
    m_opaqueDrag = value;

    /*
    Normally, QGraphicsProxyWidget handles all mouse events, so if ::acceptDrops() is true, all
    drag-n-drop handling goes to the proxied widget. Otherwise, drag-n-drop is simply disabled.
    When we set ::opaqueDrag() to true, we pass these events to the graphics scene, and in this
    case we should not handle drag-n-drop in this class (by default).
    */
    setAcceptDrops(!value);
}

bool QnMaskedProxyWidget::isUpdatesEnabled() const
{
    return m_updatesEnabled;
}

bool QnMaskedProxyWidget::event(QEvent* e)
{
    m_pixmapDirty = true;
    return QGraphicsProxyWidget::event(e);
}

void QnMaskedProxyWidget::setUpdatesEnabled(bool updatesEnabled)
{
    m_updatesEnabled = updatesEnabled;

    if (updatesEnabled && m_pixmapDirty)
        update();
}
