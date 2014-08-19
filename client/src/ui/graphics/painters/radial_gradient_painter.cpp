#include "radial_gradient_painter.h"

#include <utils/common/warnings.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
//#include <ui/graphics/shaders/color_shader_program.h>
#include <ui/graphics/shaders/per_vertex_colored_shader_program.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
#include "opengl_renderer.h"

QnRadialGradientPainter::QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor, const QGLContext *context):
    QOpenGLFunctions(context->contextHandle()),
    m_initialized(false),
    m_positionBuffer(QOpenGLBuffer::VertexBuffer)
{
    if(context != QGLContext::currentContext()) {
        qnWarning("Invalid current OpenGL context.");
        return;
    }

    m_shader = new QnPerVertexColoredGLShaderProgramm(context, NULL);
    m_shader->compile();

    QByteArray data;
    /* Generate vertex data. */
    QnGlBufferStream<GLfloat> vertexStream(&data);
    m_vertexOffset = vertexStream.offset();
    vertexStream << QVector2D(0.0f, 0.0f);
    for(int i = 0; i <= sectorCount; i++)
        vertexStream << polarToCartesian<QVector2D>(1.0f, 2 * M_PI * i / sectorCount);
    m_vertexCount = sectorCount + 2;
    qDebug() << "storing all vertices, offset" << vertexStream.offset();

    /* Generate color data. */
    QByteArray colorData;
    QnGlBufferStream<GLfloat> colorStream(&colorData);
    m_colorOffset = colorStream.offset();
    colorStream << innerColor;
    for(int i = 0; i <= sectorCount; i++)
        colorStream << outerColor;

    qDebug() << "storing all colors, offset" << colorStream.offset();

    // Create VAO for first object to render
    m_vertices.create();
    m_vertices.bind();

    // Setup VBOs and IBO (use QOpenGLBuffer to buffer data,
    // specify format, usage hint etc). These will be
    // remembered by the currently bound VAO
    m_positionBuffer.create();
    //m_positionBuffer.setUsagePattern( QOpenGLBuffer::StreamDraw );
    m_positionBuffer.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_positionBuffer.bind();

    qDebug() <<"allocate " << data.size() << "bytes to store" << m_vertexCount << "sectors and colors";
    m_positionBuffer.allocate(data.data(), data.size());

//     glVertexAttribPointer(_positionAttr, 3, GL_FLOAT, GL_FALSE, 0, 0);
//     glVertexAttribPointer(_colourAttr, 3, GL_FLOAT, GL_FALSE, 0, (void*)(_colourOffset));
// 
//     glEnableVertexAttribArray(_positionAttr);  
//     glEnableVertexAttribArray(_colourAttr);
//     

    m_positionBuffer.create();
    m_positionBuffer.setUsagePattern( QOpenGLBuffer::StreamDraw );
    m_positionBuffer.bind();
    m_positionBuffer.allocate( data.data(), data.size() );
    m_shader->enableAttributeArray( 0 );
    m_shader->setAttributeBuffer( 0, GL_FLOAT, 0, 2 );

    m_colorBuffer.create();
    m_colorBuffer.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_colorBuffer.bind();
    m_colorBuffer.allocate( colorData.data(), colorData.size());
    m_shader->enableAttributeArray( 1 );
    m_shader->setAttributeBuffer( 1, GL_FLOAT, 0, 4 );


    m_vertices.release();

    m_initialized = true;
}

QnRadialGradientPainter::~QnRadialGradientPainter() {
    if(m_initialized && QGLContext::currentContext())
        glDeleteBuffers(1, &m_buffer);
}

void QnRadialGradientPainter::paint(const QColor &colorMultiplier) {
    if(!m_initialized || !isAvailable())
        return;

    QnOpenGLRendererManager::instance(QGLContext::currentContext()).setColor(colorMultiplier);
    //QnOpenGLRendererManager::instance(QGLContext::currentContext()).drawPerVertexColoredPolygon(m_buffer,m_vertexCount);
    QnOpenGLRendererManager::instance(QGLContext::currentContext()).drawVao(&m_vertices, m_shader, m_vertexCount);

}

void QnRadialGradientPainter::paint() {
    paint(Qt::white);
}

bool QnRadialGradientPainter::isAvailable() const {
    return hasOpenGLFeature(QOpenGLFunctions::Shaders) && hasOpenGLFeature(QOpenGLFunctions::Buffers);
}
