#ifndef QN_OPENGL_RENDERER_H
#define QN_OPENGL_RENDERER_H

#include <utils/common/singleton.h>

#include <QtCore/QScopedPointer>
#include <QtOpenGL/QGLShaderProgram>


#include "ui/graphics/shaders/base_shader_program.h"
#include "ui/graphics/shaders/color_shader_program.h"
#include "ui/graphics/shaders/texture_color_shader_program.h"
#include "ui/graphics/shaders/per_vertex_colored_shader_program.h"

class QOpenGLFunctions;

class QnOpenGLRenderer : protected QOpenGLFunctions
{

public:
    QnOpenGLRenderer(const QGLContext* a_context , QObject *parent = NULL);
    
    void                setColor(const QVector4D& c);
    void                setColor(const QColor& c);

    void                drawBindedTextureOnQuad(const float* v_array, const float* tx_array);

    void                drawColoredQuad(const QRectF &rect , QnColorGLShaderProgram* shader = NULL);
    void                drawPerVertexColoredPolygon(unsigned int a_buffer , unsigned int a_vertices_size , unsigned int a_polygon_state = GL_TRIANGLE_FAN);
    void                drawColoredQuad(const float* v_array, QnColorGLShaderProgram* shader = NULL);
    
    void                drawArraysVao(QOpenGLVertexArrayObject* vao, GLenum mode, int count, QnColorGLShaderProgram* shader);
    void                drawBindedTextureOnQuadVao(QOpenGLVertexArrayObject* vao, QnGLShaderProgram* shader);

    QMatrix4x4          getModelViewMatrix() const;
    void                setModelViewMatrix(const QMatrix4x4 &matrix);
    QMatrix4x4          pushModelViewMatrix();
    void                popModelViewMatrix();

    QMatrix4x4          getProjectionMatrix() const;
    void                setProjectionMatrix(const QMatrix4x4 &matrix);

    QnColorPerVertexGLShaderProgram* getColorPerVertexShader() const;
    QnTextureGLShaderProgram* getTextureShader() const;
    QnColorGLShaderProgram* getColorShader() const;

private:
    Q_DISABLE_COPY(QnOpenGLRenderer);

    QMatrix4x4 m_modelViewMatrix;
    QStack<QMatrix4x4> m_modelViewMatrixStack;
    QMatrix4x4 m_projectionMatrix;
    QStack<QMatrix4x4> m_projectionMatrixStack;
    QVector4D  m_color; 

    unsigned short m_indices_for_render_quads[6];
    
    QScopedPointer<QnColorGLShaderProgram>          m_colorProgram;
    QScopedPointer<QnTextureGLShaderProgram>  m_textureColorProgram;
    QScopedPointer<QnColorPerVertexGLShaderProgram> m_colorPerVertexShader;
};

class QnOpenGLRendererManager: public QObject {
    Q_OBJECT;
public:
    QnOpenGLRendererManager(QObject* parent = NULL);
    ~QnOpenGLRendererManager();

    static QnOpenGLRenderer* instance(const QGLContext* a_context);

    //QHash<const QGLContext*,QnOpenGLRenderer>& getContainer(){ return m_container; };
private:
    QHash<const QGLContext*, QnOpenGLRenderer*> m_container;
};

void loadImageData( int texture_wigth , int texture_height , int image_width , int image_heigth , int gl_bytes_per_pixel , int gl_format , const uchar* pixels );



#endif // QN_SIMPLE_SHADER_PROGRAM_H
