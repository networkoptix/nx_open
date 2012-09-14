#include "shadow_item.h"
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/common/geometry.h>

QnShadowItem::QnShadowItem(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_color(QColor(Qt::black)),
    m_softWidth(0.0),
    m_shapeProvider(NULL),
    m_shapeValid(false),
    m_parametersValid(false)
{
    setAcceptedMouseButtons(0);
    
    /* Don't disable this item here. When disabled, it starts accepting wheel events 
     * (and probably other events too). Looks like a Qt bug. */
}

QnShadowItem::~QnShadowItem() {
    setShapeProvider(NULL);
}

QnShadowShapeProvider *QnShadowItem::shapeProvider() const {
    return m_shapeProvider;
}

void QnShadowItem::setShapeProvider(QnShadowShapeProvider *provider) {
    if(m_shapeProvider == provider)
        return;

    if(m_shapeProvider)
        m_shapeProvider->m_item = NULL;

    m_shapeProvider = provider;

    if(m_shapeProvider) {
        if(m_shapeProvider->m_item)
            m_shapeProvider->m_item->setShapeProvider(NULL);
        m_shapeProvider->m_item = this;
    }

    invalidateShadowShape();
}

qreal QnShadowItem::softWidth() const {
    return m_softWidth;
}

void QnShadowItem::setSoftWidth(qreal softWidth) {
    m_softWidth = softWidth;

    m_parametersValid = false;
}

const QColor &QnShadowItem::color() const {
    return m_color;
}

void QnShadowItem::setColor(const QColor &color) {
    m_color = color;
}

void QnShadowItem::invalidateShadowShape() {
    m_shapeValid = false;
    m_parametersValid = false;
}

void QnShadowItem::ensureShadowShape() const {
    if(m_shapeValid)
        return;

    if(m_shapeProvider) {
        m_shadowShape = m_shapeProvider->calculateShadowShape();
    } else {
        m_shadowShape = QPolygonF();
    }
    m_shapeValid = true;
}

void QnShadowItem::ensureShadowParameters() const {
    if(m_parametersValid)
        return;

    ensureShadowShape();

    m_painterPath = QPainterPath();
    m_painterPath.addPolygon(m_shadowShape);
    m_boundingRect = m_shadowShape.boundingRect();
    m_parametersValid = true;
}

QRectF QnShadowItem::boundingRect() const {
    ensureShadowParameters();

    return m_boundingRect;
}

QPainterPath QnShadowItem::shape() const {
    ensureShadowParameters();

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

void QnShadowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    ensureShadowShape();

    /* Color for drawing the shadow. */
    QColor color = m_color;
    color.setAlpha(color.alpha() * effectiveOpacity());

    /* Color for drawing the soft corners. */
    //QColor transparent = toTransparent(color);
    
    painter->beginNativePainting();
    //glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

    /* Draw shadowed rect. */
    glBegin(GL_TRIANGLE_FAN);
    glColor(color);
    glVertices(m_shadowShape);
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
