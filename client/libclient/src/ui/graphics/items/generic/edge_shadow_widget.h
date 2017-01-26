#pragma once

#include <QtWidgets/QGraphicsWidget>
#include <ui/customization/customized.h>

/** Simple plain shadow along one edge of a graphics scene widget. */
class QnEdgeShadowWidget : public Customized<QGraphicsWidget>
{
    Q_OBJECT
    using base_type = Customized<QGraphicsWidget>;

public:
    QnEdgeShadowWidget(QGraphicsWidget* shadowCaster, Qt::Edge edge, qreal thickness = 1.0, bool inner = false);

private:
    void at_shadowCasterGeometryChanged();

private:
    QGraphicsWidget* m_shadowCaster;
    Qt::Edge m_edge;
    qreal m_thickness;
    bool m_inner;
};
