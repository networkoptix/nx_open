#pragma once

#include <QtWidgets/QGraphicsWidget>
#include <ui/customization/customized.h>

/** Simple plain shadow along one edge of a graphics scene widget. */
class QnEdgeShadowWidget : public Customized<QGraphicsWidget>
{
    Q_OBJECT
    using base_type = Customized<QGraphicsWidget>;

public:
    enum Location
    {
        kInner,
        kOuter
    };

    Q_ENUM(Location);

    QnEdgeShadowWidget(
        QGraphicsItem* parent,          //< an item containing the shadow
        QGraphicsWidget* shadowCaster,  //< widget to which geometry the shadow is aligned
        Qt::Edge edge,                  //< at which edge of the caster the shadow resides
        qreal thickness = 1.0,          //< thickness of the shadow
        Location location = kOuter);    //< location of the shadow at the edge

private:
    void at_shadowCasterGeometryChanged();

private:
    QGraphicsWidget* m_shadowCaster;
    Qt::Edge m_edge;
    qreal m_thickness;
    Location m_location;
};
