#include "polygonal_shadow_item.h"
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/common/scene_utility.h>

QnPolygonalShadowItem::QnPolygonalShadowItem(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_softWidth(0.0),
    m_color(QColor(Qt::black)),
    m_shapeValid(true),
    m_parametersValid(false)
{
    setAcceptedMouseButtons(0);
    
    /* Don't disable this item here. When disabled, it starts accepting wheel events 
     * (and probably other events too). Looks like a Qt bug. */
}

QRectF QnPolygonalShadowItem::boundingRect() const {
    ensureParameters();

    return m_boundingRect;
}

QPainterPath QnPolygonalShadowItem::shape() const {
    ensureParameters();

    return m_painterPath;
}

namespace {
    void drawSoftBand(const QPointF &v, const QPointF &dt, const QPointF &dw, const QColor &normal, const QColor &transparent) {
        glBegin(GL_QUADS);
        glColor(transparent);
        glVertex(v + dt);
        glVertex(v + dt + dw);
        glColor(normal);
        glVertex(v + dw);
        glVertex(v);
        glEnd();
    }

    void drawSoftCorner(const QPointF &v, const QPointF &dx, const QPointF &dy, const QColor &normal, const QColor &transparent) {
        glBegin(GL_TRIANGLE_FAN);
        glColor(normal);
        glVertex(v);
        glColor(transparent);
        glVertex(v + dx);
        glVertex(v + (dx + dy) * 0.7);
        glVertex(v + dy);
        glEnd();
    }
}

void QnPolygonalShadowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    ensureParameters();

    /* Color for drawing the shadow. */
    QColor color = m_color;
    color.setAlpha(color.alpha() * effectiveOpacity());

    /* Color for drawing the soft corners. */
    QColor transparent = toTransparent(color);
    
    painter->beginNativePainting();
    //glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

    /* Draw shadowed rect. */
    glBegin(GL_TRIANGLE_FAN);
    glColor(color);
    glVertices(m_shape);
    glEnd();

    /* Draw soft band. */
    // TODO
    /*
    if(!qFuzzyIsNull(m_softWidth)) {
        QPointF dx(m_softWidth, 0), dy(0, m_softWidth);
        QPointF w(m_rect.width(), 0), h(0, m_rect.height());

        drawSoftBand(m_rect.topLeft(),       -dy,   w, m_color, transparent);
        drawSoftBand(m_rect.topRight(),       dx,   h, m_color, transparent);
        drawSoftBand(m_rect.bottomRight(),    dy,  -w, m_color, transparent);
        drawSoftBand(m_rect.bottomLeft(),    -dx,  -h, m_color, transparent);

        drawSoftCorner(m_rect.topLeft(),     -dx, -dy, m_color, transparent);
        drawSoftCorner(m_rect.topRight(),     dx, -dy, m_color, transparent);
        drawSoftCorner(m_rect.bottomRight(),  dx,  dy, m_color, transparent);
        drawSoftCorner(m_rect.bottomLeft(),  -dx,  dy, m_color, transparent);
    }*/

    glDisable(GL_BLEND); 
    //glPopAttrib();
    painter->endNativePainting();
}

void QnPolygonalShadowItem::invalidateShape() {
    m_shapeValid = false;
    m_parametersValid = false;
}

void QnPolygonalShadowItem::ensureParameters() const {
    if(!m_shapeValid && m_shapeProvider != NULL) {
        m_shape = m_shapeProvider->provideShape();
        m_shapeValid = true;
    }

    if(!m_parametersValid) {
        m_painterPath = QPainterPath();
        m_painterPath.addPolygon(m_shape);
        m_boundingRect = m_shape.boundingRect();
        m_parametersValid = true;
    }
}

void QnPolygonalShadowItem::setShapeProvider(QnPolygonalShapeProvider *provider) {
    m_shapeProvider = provider;

    ensureParameters();
}

void QnPolygonalShadowItem::setShape(const QPolygonF &shape) {
    m_shape = shape;

    m_shapeValid = true;
    m_parametersValid = false;
}

void QnPolygonalShadowItem::setSoftWidth(qreal softWidth) {
    m_softWidth = softWidth;

    m_parametersValid = false;
}

void QnPolygonalShadowItem::setColor(const QColor &color) {
    m_color = color;
}