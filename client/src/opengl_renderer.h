#ifndef QN_OPENGL_RENDERER_H
#define QN_OPENGL_RENDERER_H

#include <utils/common/singleton.h>
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
    void                drawBindedTextureOnQuad(const QRectF &rect , QnTextureColorGLShaderProgramm* shader = NULL);
    void                drawBindedTextureOnQuad(const QRectF &rect , const QSizeF& size, QnTextureColorGLShaderProgramm* shader = NULL);
    void                drawColoredPolygon(const QPolygonF & a_polygon, QnColorGLShaderProgramm* shader = NULL);
    void                drawColoredPolygon(const float* v_array, unsigned int size , QnColorGLShaderProgramm* shader = NULL);
    void                drawColoredPolygon(const QVector<QVector2D>& a_polygon, QnColorGLShaderProgramm* shader = NULL);
    void                drawColoredQuad(const QRectF &rect , QnColorGLShaderProgramm* shader = NULL);
    void                drawPerVertexColoredPolygon(unsigned int a_buffer , unsigned int a_vertices_size , unsigned int a_polygon_state = GL_TRIANGLE_FAN);
    void                drawBindedTextureOnQuad(const float* v_array, const float* tx_array, QnAbstractBaseGLShaderProgramm* shader = NULL );
    void                drawColoredQuad(const float* v_array, QnColorGLShaderProgramm* shader = NULL);
    void                drawVao(QOpenGLVertexArrayObject* vao, int count);

    QMatrix4x4&         getModelViewMatrix() { return m_modelViewMatrix; };
    const QMatrix4x4&   getModelViewMatrix() const { return m_modelViewMatrix; };

    QMatrix4x4&         getProjectionMatrix() { return m_projectionMatrix; };
    const QMatrix4x4&   getProjectionMatrix() const { return m_projectionMatrix; };

    QnPerVertexColoredGLShaderProgramm* getColorShader() const;
private:
    QMatrix4x4 m_modelViewMatrix;
    QMatrix4x4 m_projectionMatrix;
    QVector4D  m_color; 

    unsigned short m_indices_for_render_quads[6];
    
    QSharedPointer<QnColorGLShaderProgramm>        m_colorProgram;
    QSharedPointer<QnTextureColorGLShaderProgramm> m_textureColorProgram;
    QSharedPointer<QnPerVertexColoredGLShaderProgramm> m_texturePerVertexColoredProgram;
};

class QnOpenGLRendererManager: public QObject {
    Q_OBJECT;
public:
    static QnOpenGLRenderer& instance(const QGLContext* a_context);

    //QHash<const QGLContext*,QnOpenGLRenderer>& getContainer(){ return m_container; };
private:
    QHash<const QGLContext*,QnOpenGLRenderer> m_container;
};

void loadImageData( int texture_wigth , int texture_height , int image_width , int image_heigth , int gl_bytes_per_pixel , int gl_format , const uchar* pixels );



#endif // QN_SIMPLE_SHADER_PROGRAM_H
