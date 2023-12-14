// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "opengl_renderer.h"

#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include "ui/graphics/opengl/gl_shortcuts.h"

#include <ui/graphics/shaders/base_shader_program.h>
#include <ui/graphics/shaders/blur_shader_program.h>
#include <ui/graphics/shaders/color_shader_program.h>
#include <ui/graphics/shaders/texture_color_shader_program.h>
#include <ui/graphics/shaders/per_vertex_colored_shader_program.h>
#include <ui/graphics/shaders/texture_transition_shader_program.h>
#include <ui/graphics/shaders/color_line_shader_program.h>

QnOpenGLRenderer::QnOpenGLRenderer(QObject *parent):
    m_colorProgram(new QnColorGLShaderProgram(parent)),
    m_textureColorProgram(new QnTextureGLShaderProgram(parent)),
    m_colorPerVertexShader(new QnColorPerVertexGLShaderProgram(parent)),
    m_textureTransitionShader(new QnTextureTransitionShaderProgram(parent)),
    m_colorLineShader(new QnColorLineGLShaderProgram(parent)),
    m_blurShader(new QnBlurShaderProgram(parent))
{
    QOpenGLFunctions::initializeOpenGLFunctions();

    m_colorProgram->compile();
    m_textureColorProgram->compile();
    m_colorPerVertexShader->compile();
    m_textureTransitionShader->compile();
    m_colorLineShader->compile();
    m_blurShader->compile();

    m_indices_for_render_quads[0] = 0;
    m_indices_for_render_quads[1] = 1;
    m_indices_for_render_quads[2] = 2;

    m_indices_for_render_quads[3] = 0;
    m_indices_for_render_quads[4] = 2;
    m_indices_for_render_quads[5] = 3;

    glGenBuffers(1, &m_elementBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        sizeof(m_indices_for_render_quads),
        m_indices_for_render_quads,
        GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_color = QVector4D(1.0f,1.0f,1.0f,1.0f);
    m_modelViewMatrix.setToIdentity();
    m_projectionMatrix.setToIdentity();
}

QnOpenGLRenderer::~QnOpenGLRenderer()
{
    glDeleteBuffers(1, &m_elementBuffer);
}

void  QnOpenGLRenderer::setColor(const QVector4D& c)
{
    m_color = c;
};

void  QnOpenGLRenderer::setColor(const QColor& c)
{
    m_color.setX(c.redF());
    m_color.setY(c.greenF());
    m_color.setZ(c.blueF());
    m_color.setW(c.alphaF());
};

void QnOpenGLRenderer::drawColoredQuad( const QRectF &rect , QnColorGLShaderProgram* shader )
{
    GLfloat vertices[] = {
        (GLfloat)rect.left(),   (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.bottom(),
        (GLfloat)rect.left(),   (GLfloat)rect.bottom()
    };
    drawColoredQuad(vertices,shader);
};

void QnOpenGLRenderer::drawPerVertexColoredPolygon( unsigned int a_buffer , unsigned int a_vertices_size , unsigned int a_polygon_state)
{
    QnColorPerVertexGLShaderProgram* shader = m_colorPerVertexShader.data();

    const int VERTEX_POS_SIZE = 2; // x, y
    const int VERTEX_COLOR_SIZE = 4; // s and t
    const int VERTEX_POS_INDX = 0;
    const int VERTEX_COLOR_INDX = 1;
    glBindBuffer(GL_ARRAY_BUFFER, a_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(VERTEX_POS_INDX);
    glEnableVertexAttribArray(VERTEX_COLOR_INDX);

    shader->bind();
    shader->setModelViewProjectionMatrix(m_projectionMatrix*m_modelViewMatrix);
    shader->setColor(m_color);

    if ( !shader->initialized() )
    {
        shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
        shader->bindAttributeLocation("aColor",VERTEX_COLOR_INDX);
        shader->markInitialized();
    };

    const char* ptr = nullptr;
    GLuint offset = 0;
    glVertexAttribPointer(VERTEX_POS_INDX, VERTEX_POS_SIZE, GL_FLOAT, GL_FALSE, 0, (const void*)(ptr + offset));
    offset += a_vertices_size*VERTEX_POS_SIZE* sizeof(GLfloat);
    glVertexAttribPointer(VERTEX_COLOR_INDX,VERTEX_COLOR_SIZE, GL_FLOAT,GL_FALSE, 0, (const void*)(ptr + offset));

    glDrawArrays(a_polygon_state,0,a_vertices_size);

    shader->release();
    glCheckError("render");
    glBindBuffer(GL_ARRAY_BUFFER, 0);

}

void QnOpenGLRenderer::drawArraysVao(QOpenGLVertexArrayObject* vao, GLenum mode, int count, QnColorGLShaderProgram* shader) {
    vao->bind();

    shader->bind();
    shader->setModelViewProjectionMatrix(m_projectionMatrix*m_modelViewMatrix);
    shader->setColor(m_color);

    glDrawArrays(mode, 0, count);
    vao->release();
    glCheckError("render");
}

void QnOpenGLRenderer::drawColoredQuad(const float* v_array, QnColorGLShaderProgram* shader)
{
    if ( !shader )
        shader = m_colorProgram.data();
    if ( shader )
    {
        const int VERTEX_POS_SIZE = 2; // x, y
        const int VERTEX_POS_INDX = 0;
        glCheckError("render");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glEnableVertexAttribArray(VERTEX_POS_INDX);
        glVertexAttribPointer(VERTEX_POS_INDX, VERTEX_POS_SIZE, GL_FLOAT, GL_FALSE, 0, v_array);
        glCheckError("render");


        shader->bind();
        shader->setModelViewProjectionMatrix(m_projectionMatrix*m_modelViewMatrix);
        shader->setColor(m_color);
        if ( !shader->initialized()  )
        {
            shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
            shader->markInitialized();
        };

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementBuffer);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glCheckError("render");
        shader->release();
        //glDisableVertexAttribArray(VERTEX_POS_INDX);
    }
};

void QnOpenGLRenderer::drawBindedTextureOnQuadVao(QOpenGLVertexArrayObject* vao, QnGLShaderProgram* shader) {
    vao->bind();
    shader->setModelViewProjectionMatrix(m_projectionMatrix*m_modelViewMatrix);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementBuffer);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    vao->release();
    glCheckError("render");
}

QnColorPerVertexGLShaderProgram* QnOpenGLRenderer::getColorPerVertexShader() const {
    return m_colorPerVertexShader.data();
}

QnTextureGLShaderProgram* QnOpenGLRenderer::getTextureShader() const {
    return m_textureColorProgram.data();
}

QnColorGLShaderProgram* QnOpenGLRenderer::getColorShader() const {
    return m_colorProgram.data();
}

QnTextureTransitionShaderProgram* QnOpenGLRenderer::getTextureTransitionShader() const {
    return m_textureTransitionShader.data();
}

QnColorLineGLShaderProgram* QnOpenGLRenderer::getColorLineShader() const
{
    return m_colorLineShader.data();
}

QnBlurShaderProgram* QnOpenGLRenderer::getBlurShader() const
{
    return m_blurShader.data();
}

QMatrix4x4 QnOpenGLRenderer::getModelViewMatrix() const {
    return m_modelViewMatrix;
}

void QnOpenGLRenderer::setModelViewMatrix(const QMatrix4x4 &matrix) {
    m_modelViewMatrix = matrix;
}

QMatrix4x4 QnOpenGLRenderer::getProjectionMatrix() const {
    return m_projectionMatrix;
}

void QnOpenGLRenderer::setProjectionMatrix(const QMatrix4x4 &matrix) {
    m_projectionMatrix = matrix;
}

QMatrix4x4 QnOpenGLRenderer::pushModelViewMatrix() {
    m_modelViewMatrixStack.push(m_modelViewMatrix);
    return m_modelViewMatrix;
}

void QnOpenGLRenderer::popModelViewMatrix() {
    m_modelViewMatrix = m_modelViewMatrixStack.pop();
}

// =================================================================================================

Q_GLOBAL_STATIC(QnOpenGLRendererManager, qn_openGlRenderManager_instance)

QnOpenGLRendererManager::RendererPtr QnOpenGLRendererManager::instance(QOpenGLWidget* glWidget)
{
    return qn_openGlRenderManager_instance()->get(glWidget);
}

QnOpenGLRendererManager::RendererPtr QnOpenGLRendererManager::get(QOpenGLWidget* glWidget)
{
    auto it = m_container.find(glWidget);
    if (it != m_container.end())
        return *it;

    const auto renderer = RendererPtr(new QnOpenGLRenderer());
    m_container[glWidget] = renderer;

    connect(glWidget, &QObject::destroyed, this,
        [this, glWidget]() { m_container.remove(glWidget); });

    return renderer;
}

QnOpenGLRendererManager::QnOpenGLRendererManager(QObject* parent /* = nullptr*/):
    QObject(parent)
{
}

QnOpenGLRendererManager::~QnOpenGLRendererManager()
{
}

void loadImageData(
    QOpenGLFunctions* renderer,
    int texture_wigth,
    int texture_height,
    int image_width,
    int image_heigth,
    int gl_bytes_per_pixel,
    int gl_format,
    const uchar* pixels)
{
    if (!renderer)
        return;

    Q_UNUSED(texture_wigth);
    Q_UNUSED(gl_bytes_per_pixel);

    // if ( texture_wigth >= image_width )
    {
        renderer->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_width,
            qMin(image_heigth, texture_height), gl_format, GL_UNSIGNED_BYTE, pixels);
        glCheckError("glTexSubImage2D");
    }

    /*
       Following check reduces amount of data to be loaded to video card by the cost of increased GPU consumption.
       On the practice, data difference is very low but GPU consumption is very high, especially on integrated video.
       Disabling it.
       --sivanov
    */
    /*else if (texture_wigth < image_width)
    {
        int h = qMin(image_heigth,texture_height);
        for( int y = 0; y < h; y++ )
        {
            const uchar *row = pixels + (y*image_width) * gl_bytes_per_pixel;
            renderer->glTexSubImage2D( GL_TEXTURE_2D, 0, 0, y , texture_wigth, 1, gl_format, GL_UNSIGNED_BYTE, row );
            glCheckError("glTexSubImage2D");
        }

    }*/
}
