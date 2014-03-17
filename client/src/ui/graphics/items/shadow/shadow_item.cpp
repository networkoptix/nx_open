#include "shadow_item.h"

#include <utils/common/scoped_painter_rollback.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/common/geometry.h>
#include <ui/workaround/gl_native_painting.h>

QnShadowItem::QnShadowItem(QGraphicsItem *parent):
    base_type(parent),
    m_color(QColor(0, 0, 0, 128)),
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

void QnShadowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    ensureShadowShape();

#if 0
    QN_SCOPED_PAINTER_BRUSH_ROLLBACK(painter, m_color);
    QN_SCOPED_PAINTER_PEN_ROLLBACK(painter, Qt::NoPen);
    painter->drawPolygon(m_shadowShape);
#else
    /* This code actually works faster. 
     * On a scene with 64 simple items I get 20-30% FPS increase. */

    /* Color for drawing the shadow. */
    QColor color = m_color;
    color.setAlpha(color.alpha() * effectiveOpacity());

    QnGlNativePainting::begin(painter);
    //glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

    /* Draw shadowed rect. */
    glBegin(GL_TRIANGLE_FAN);
    glColor(color);
    glVertices(m_shadowShape);
    glEnd();

    glDisable(GL_BLEND); 
    //glPopAttrib();
    QnGlNativePainting::end(painter);
#endif
}
