
#include "caching_proxy_widget.h"

#include <cassert>

#include <QtGui/qopengl.h>

#include <QtCore/QEvent>
#include <QtCore/QSysInfo>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/geometry.h>
#include <ui/workaround/gl_native_painting.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>
#include "opengl_renderer.h"

CachingProxyWidget::CachingProxyWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    QGraphicsProxyWidget(parent, windowFlags),
    m_maxTextureSize(QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE)),
    m_texture(0),
    m_wholeTextureDirty(true),
    m_initialized(false)
{}

CachingProxyWidget::~CachingProxyWidget() {
    if(m_texture != 0)
        glDeleteTextures(1, &m_texture);
}

void CachingProxyWidget::ensureTextureAllocated() {
    if(m_texture != 0)
        return;

    glGenTextures(1, &m_texture);

    /* Set texture parameters. */
//    glPushAttrib(GL_ENABLE_BIT);
    //Deprecated in OpenGL ES2.0
    //glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
//    glPopAttrib();
}

void CachingProxyWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget * ) {
    if (painter->paintEngine() == NULL) {
        qnWarning("No OpenGL-compatible paint engine was found.");
        return;
    }

    if (painter->paintEngine()->type() != QPaintEngine::OpenGL2 && painter->paintEngine()->type() != QPaintEngine::OpenGL) {
        qnWarning("Painting with the paint engine of type '%1' is not supported", static_cast<int>(painter->paintEngine()->type()));
        return;
    }
    ensureTextureAllocated();
    
    ensureCurrentWidgetSynchronized();
    if(currentWidget() == NULL || !currentWidget()->isVisible())
        return;

    const QRect exposedWidgetRect = (option->exposedRect & rect()).toAlignedRect();
    if (exposedWidgetRect.isEmpty())
        return;


    QRectF vertexRect = this->rect();
    QRectF textureRect = QRectF(
        QPointF(0.0, 0.0),
        QnGeometry::cwiseDiv(vertexRect.size(), m_image.size())
        );

    GLfloat vertices[] = {
        (GLfloat)vertexRect.left(),   (GLfloat)vertexRect.top(),
        (GLfloat)vertexRect.right(),  (GLfloat)vertexRect.top(),
        (GLfloat)vertexRect.right(),  (GLfloat)vertexRect.bottom(),
        (GLfloat)vertexRect.left(),   (GLfloat)vertexRect.bottom()
    };

    QSizeF size = QnGeometry::cwiseDiv(vertexRect.size(), m_image.size());
    GLfloat texCoords[] = {
        0.0, 0.0,
        (GLfloat)size.width(), 0.0,
        (GLfloat)size.width(), (GLfloat)size.height(),
        0.0, (GLfloat)size.height()
    };

    QByteArray data, texData;
    QnGlBufferStream<GLfloat> vertexStream(&data), texStream(&texData);

    for(int i = 0; i < 8; i++) {
        vertexStream << vertices[i];
        texStream << texCoords[i];
    }

    const int VERTEX_POS_INDX = 0;
    const int VERTEX_TEXCOORD0_INDX = 1;
    const int VERTEX_POS_SIZE = 2; // x, y
    const int VERTEX_TEXCOORD0_SIZE = 2; // s and t

    auto shader = QnOpenGLRendererManager::instance(QGLContext::currentContext())->getTextureShader();

    if (!m_initialized) {
        m_vertices.create();
        m_vertices.bind();

        m_positionBuffer.create();
        m_positionBuffer.setUsagePattern( QOpenGLBuffer::DynamicDraw );
        m_positionBuffer.bind();
        m_positionBuffer.allocate( data.data(), data.size() );
        shader->enableAttributeArray( VERTEX_POS_INDX );
        shader->setAttributeBuffer( VERTEX_POS_INDX, GL_FLOAT, 0, VERTEX_POS_SIZE );

        m_textureBuffer.create();
        m_textureBuffer.setUsagePattern( QOpenGLBuffer::DynamicDraw );
        m_textureBuffer.bind();
        m_textureBuffer.allocate( texData.data(), texData.size());
        shader->enableAttributeArray( VERTEX_TEXCOORD0_INDX );
        shader->setAttributeBuffer( VERTEX_TEXCOORD0_INDX, GL_FLOAT, 0, VERTEX_TEXCOORD0_SIZE );

        if (!shader->initialized()) {
            shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
            shader->bindAttributeLocation("aTexcoord",VERTEX_TEXCOORD0_INDX);
            shader->markInitialized();
        };    

        m_positionBuffer.release();
        m_textureBuffer.release();
        m_vertices.release();

        m_initialized = true;
    } else {
        m_positionBuffer.bind();
        m_positionBuffer.write(0, data.data(), data.size());
        m_positionBuffer.release();

        m_textureBuffer.bind();
        m_textureBuffer.write(0, texData.data(), texData.size());
        m_textureBuffer.release();

    }


    QnGlNativePainting::begin(QGLContext::currentContext(),painter);

    ensureTextureSynchronized();

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader->bind();
    shader->setColor(QVector4D(1.0f, 1.0f, 1.0f, effectiveOpacity()));
    shader->setTexture(0);
    QnOpenGLRendererManager::instance(QGLContext::currentContext())->drawBindedTextureOnQuadVao(&m_vertices, shader);
    shader->release();

    glDisable(GL_BLEND);

    QnGlNativePainting::end(painter);
}

bool CachingProxyWidget::eventFilter(QObject *object, QEvent *event) {
    if(event->type() == QEvent::UpdateRequest && object == currentWidget()) {
        m_wholeTextureDirty = true;
    }
    
    return base_type::eventFilter(object, event);
}

void CachingProxyWidget::ensureCurrentWidgetSynchronized() {
    if(currentWidget() == widget())
        return;

    m_wholeTextureDirty = true;

    if(currentWidget() != NULL)
        currentWidget()->removeEventFilter(this);

    m_currentWidget = widget();

    if(currentWidget() != NULL)
        currentWidget()->installEventFilter(this);
}

void CachingProxyWidget::ensureTextureSizeSynchronized() {
    assert(currentWidget() != NULL);

    if(QnGeometry::contains(m_image.size(), currentWidget()->size()))
        return;

    if(m_image.width() == m_maxTextureSize)
        return;

    int widgetSide = qMax(currentWidget()->width(), currentWidget()->height());
    int textureSide = 256;
    for(; textureSide <= m_maxTextureSize; textureSide *= 2)
        if(textureSide >= widgetSide)
            break;
    textureSide = qMin(textureSide, m_maxTextureSize);

    m_image = QImage(textureSide, textureSide, QImage::Format_ARGB32);
    m_wholeTextureDirty = true;
}

void CachingProxyWidget::ensureTextureSynchronized() {
    assert(currentWidget() != NULL);

    ensureTextureSizeSynchronized();

    if(!m_wholeTextureDirty)
        return;
    
    currentWidget()->render(&m_image, QPoint(0, 0), currentWidget()->rect());
    //Deprecated in OpenGL ES2.0
    //glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
//    glPixelStorei(GL_UNPACK_ROW_LENGTH, m_image.bytesPerLine() * 8 / m_image.depth());

    /* QImage's Format_ARGB32 corresponds to OpenGL's GL_BGRA on little endian architectures.
     * On big endian architectures, conversion is needed. */
    assert(QSysInfo::ByteOrder == QSysInfo::LittleEndian);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_image.width(), m_image.height(), 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, m_image.bits());
    unsigned int row_width = m_image.bytesPerLine() * 8 / m_image.depth();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_image.width(), m_image.height(), 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
    loadImageData(m_image.width(), m_image.height(),row_width,m_image.height(),4,GL_BGRA_EXT,m_image.bits());
    /*
    if ( m_image.width() == row_width )
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_image.width(), m_image.height(), 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, m_image.bits());
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_image.width(), m_image.height(), 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
        for( int y = 0; y < m_image.height(); y++ )
        {
            const uchar *row = m_image.bits() + (y*row_width) * 4;
            glTexSubImage2D( GL_TEXTURE_2D, 0, 0, y , m_image.width(), 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, row );
        }
    }*/



//    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    /* No need to restore OpenGL state here, it is done by the caller. */

    m_wholeTextureDirty = false;
}


