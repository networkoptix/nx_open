#include "edge_shadow_widget.h"

#include <nx/utils/log/assert.h>


QnEdgeShadowWidget::QnEdgeShadowWidget(
            QGraphicsItem* parent,
            QGraphicsWidget* shadowCaster,
            Qt::Edge edge,
            qreal thickness,
            Location location)
    :
    base_type(parent),
    m_shadowCaster(shadowCaster),
    m_edge(edge),
    m_thickness(thickness),
    m_location(location)
{
    NX_ASSERT(m_shadowCaster, Q_FUNC_INFO, "Caster should not be null");
    if (!m_shadowCaster)
        return;

    setAutoFillBackground(true);

    setVisible(m_shadowCaster->isVisible());
    setOpacity(m_shadowCaster->opacity());

    at_shadowCasterGeometryChanged();

    connect(m_shadowCaster, &QGraphicsObject::visibleChanged,
        this, [this]() { setVisible(m_shadowCaster->isVisible()); });

    connect(m_shadowCaster, &QGraphicsObject::opacityChanged,
        this, [this]() { setOpacity(m_shadowCaster->opacity()); });

    connect(m_shadowCaster, &QGraphicsWidget::geometryChanged,
        this, &QnEdgeShadowWidget::at_shadowCasterGeometryChanged);
}

void QnEdgeShadowWidget::at_shadowCasterGeometryChanged()
{
    if (!parentItem())
        return;

    const auto delta = m_location == kInner ? 0.0 : m_thickness;

    /* Bounding rect of shadow caster geometry in shadow parent coordinates: */
    const auto shadowCasterGeom = [this, parent = parentItem()]() -> QRectF
        {
            const auto rect = m_shadowCaster->rect();

            if (parent == m_shadowCaster)
                return rect;

            if (m_shadowCaster->sceneTransform().type() > QTransform::TxScale
                && parent->sceneTransform().type() > QTransform::TxScale)
            {
                /* If rotations are different the shadow will be along
                 * bounding rect edge, not shadow casters edge. */
                return parent->mapFromScene(m_shadowCaster->mapToScene(rect)).boundingRect();
            }

            return QRectF(
                parent->mapFromScene(m_shadowCaster->mapToScene(rect.topLeft())),
                parent->mapFromScene(m_shadowCaster->mapToScene(rect.bottomRight())));
        }();

    switch (m_edge)
    {
        case Qt::LeftEdge:
            setGeometry({ shadowCasterGeom.topLeft() - QPointF(delta, 0.0),
                QSizeF(m_thickness, shadowCasterGeom.height()) });
            break;

        case Qt::RightEdge:
            setGeometry({ shadowCasterGeom.topRight() - QPointF(m_thickness - delta, 0.0),
                QSizeF(m_thickness, shadowCasterGeom.height()) });
            break;

        case Qt::TopEdge:
            setGeometry({ shadowCasterGeom.topLeft() - QPointF(0.0, delta),
                QSizeF(shadowCasterGeom.width(), m_thickness) });
            break;

        case Qt::BottomEdge:
            setGeometry({ shadowCasterGeom.bottomLeft() - QPointF(0.0, m_thickness - delta),
                QSizeF(shadowCasterGeom.width(), m_thickness) });
            break;
    }
}
