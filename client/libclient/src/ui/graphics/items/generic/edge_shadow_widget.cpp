#include "edge_shadow_widget.h"

#include <nx/utils/log/assert.h>


QnEdgeShadowWidget::QnEdgeShadowWidget(QGraphicsWidget* shadowCaster,
        Qt::Edge edge, qreal thickness, bool inner) :
    base_type(shadowCaster),
    m_shadowCaster(shadowCaster),
    m_edge(edge),
    m_thickness(thickness),
    m_inner(inner)
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
    auto delta = m_inner ? 0.0 : m_thickness;
    auto shadowCasterRect = m_shadowCaster->rect();

    switch (m_edge)
    {
        case Qt::LeftEdge:
            setGeometry({ shadowCasterRect.topLeft() - QPointF(delta, 0.0),
                QSizeF(m_thickness, shadowCasterRect.height()) });
            break;

        case Qt::RightEdge:
            setGeometry({ shadowCasterRect.topRight() - QPointF(m_thickness - delta, 0.0),
                QSizeF(m_thickness, shadowCasterRect.height()) });
            break;

        case Qt::TopEdge:
            setGeometry({ shadowCasterRect.topLeft() - QPointF(0.0, delta),
                QSizeF(shadowCasterRect.width(), m_thickness) });
            break;

        case Qt::BottomEdge:
            setGeometry({ shadowCasterRect.bottomLeft() - QPointF(0.0, m_thickness - delta),
                QSizeF(shadowCasterRect.width(), m_thickness) });
            break;
    }
}
