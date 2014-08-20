#include "shadow_item.h"

#include <utils/common/scoped_painter_rollback.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>

#include <ui/common/geometry.h>
#include <ui/workaround/gl_native_painting.h>

#include "opengl_renderer.h"

QnShadowItem::QnShadowItem(QGraphicsItem *parent):
    base_type(parent),
    m_color(QColor(0, 0, 0, 128)),
    m_shapeProvider(NULL),
    m_shapeValid(false),
    m_parametersValid(false),
    m_vaoInitialized(false),
    m_positionBuffer(QOpenGLBuffer::VertexBuffer)
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
    m_vaoInitialized = false;
}

void QnShadowItem::ensureShadowShape() const {
    if(m_shapeValid)
        return;

    if(m_shapeProvider) {
        m_shadowShape = m_shapeProvider->calculateShadowShape();
    } else {
        m_shadowShape = QPolygonF();
    }
    m_shapeValid = m_shadowShape.size() > 0;
}

void QnShadowItem::initializeVao() {
    if (!m_shapeValid) 
        return;

    /* Generate vertex data. */
    QByteArray data;
    QnGlBufferStream<GLfloat> vertexStream(&data);
    for ( int i = 0 ; i < m_shadowShape.size() ; i++)
        vertexStream << m_shadowShape[i].x() << m_shadowShape[i].y();

    if (m_vertices.isCreated())
        m_vertices.destroy();

    if (m_positionBuffer.isCreated())
        m_positionBuffer.destroy();

    m_vertices.create();
    m_vertices.bind();

    const int VERTEX_POS_SIZE = 2; // x, y
    const int VERTEX_POS_INDX = 0;

    auto shader = QnOpenGLRendererManager::instance(QGLContext::currentContext())->getColorShader();

    m_positionBuffer.create();
    m_positionBuffer.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_positionBuffer.bind();
    m_positionBuffer.allocate( data.data(), data.size() );
    shader->enableAttributeArray( VERTEX_POS_INDX );
    shader->setAttributeBuffer( VERTEX_POS_INDX, GL_FLOAT, 0, VERTEX_POS_SIZE );

    if (!shader->initialized()) {
        shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
        shader->markInitialized();
    };

    m_vertices.release();

    m_vaoInitialized = true;
}


void QnShadowItem::ensureShadowParameters() const {
    if(m_parametersValid)
        return;

    ensureShadowShape();
    if (!m_shapeValid)
        return;

    m_painterPath = QPainterPath();
    m_painterPath.addPolygon(m_shadowShape);
    m_boundingRect = m_shadowShape.boundingRect();
    m_parametersValid = true;
}

QRectF QnShadowItem::boundingRect() const {
    ensureShadowParameters();
    if (m_shapeValid)
        return m_boundingRect;
    return QRect();
}

QPainterPath QnShadowItem::shape() const {
    ensureShadowParameters();
    if (m_shapeValid)
        return m_painterPath;
    return QPainterPath();
}

void QnShadowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    ensureShadowShape();

    if (!m_shapeValid)
        return;

    if (!m_vaoInitialized)
        initializeVao();

    /* Color for drawing the shadow. */
    QColor color = m_color;
    color.setAlpha(color.alpha() * effectiveOpacity());
    
    QnGlNativePainting::begin(QGLContext::currentContext(),painter);
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
   
    QnOpenGLRendererManager::instance(QGLContext::currentContext())->setColor(color);
    auto shader = QnOpenGLRendererManager::instance(QGLContext::currentContext())->getColorShader();
    /* Draw shadowed rect. */
    QnOpenGLRendererManager::instance(QGLContext::currentContext())->drawArraysVao(&m_vertices, GL_TRIANGLE_FAN, m_shadowShape.size(), shader);

    glDisable(GL_BLEND); 
    QnGlNativePainting::end(painter);
}
