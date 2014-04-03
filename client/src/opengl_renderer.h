#ifndef QN_OPENGL_RENDERER_H
#define QN_OPENGL_RENDERER_H

#include <utils/common/singleton.h>
#include <QtOpenGL/QGLShaderProgram>

#ifndef QT_OPENGL_ES_2
#include "ui/graphics/shaders/base_shader_program.h"
#include "ui/graphics/shaders/color_shader_program.h"
#include "ui/graphics/shaders/texture_color_shader_program.h"
#include "ui/graphics/shaders/per_vertex_colored_shader_program.h"
#else 
#include "ui/graphics/es_shaders/es_base_shader_program.h"
#include "ui/graphics/es_shaders/es_color_shader_program.h"
#include "ui/graphics/es_shaders/es_texture_color_shader_program.h"
#include "ui/graphics/es_shaders/es_per_vertex_colored_shader_program.h"
#endif


class QnOpenGLRenderer
{

public:
    QnOpenGLRenderer(const QGLContext* a_context , QObject *parent = NULL);
    
    void                setColor(const QVector4D& c);
    void                setColor(const QColor& c);
    void                drawBindedTextureOnQuad( const QRectF &rect , QnTextureColorGLShaderProgramm* shader = NULL);
    void                drawBindedTextureOnQuad( const QRectF &rect , const QSizeF& size, QnTextureColorGLShaderProgramm* shader = NULL);
    void                drawColoredPolygon( const QPolygonF & a_polygon, QnColorGLShaderProgramm* shader = NULL);
    void                drawColoredPolygon( const float* v_array, unsigned int size , QnColorGLShaderProgramm* shader = NULL);
    void                drawColoredPolygon( const QVector<QVector2D>& a_polygon, QnColorGLShaderProgramm* shader = NULL);
    void                drawColoredQuad( const QRectF &rect , QnColorGLShaderProgramm* shader = NULL);
    void                drawPerVertexColoredPolygon( unsigned int a_buffer , unsigned int a_vertices_size );
    void                drawBindedTextureOnQuad( const float* v_array, const float* tx_array, QnAbstractBaseGLShaderProgramm* shader = NULL );
    void                drawColoredQuad( const float* v_array, QnColorGLShaderProgramm* shader = NULL );

    QMatrix4x4&         getModelViewMatrix(){ return m_modelViewMatrix; };
    const QMatrix4x4&   getModelViewMatrix() const { return m_modelViewMatrix; };

    QMatrix4x4&         getProjectionMatrix(){ return m_projectionMatrix; };
    const QMatrix4x4&   getProjectionMatrix() const { return m_projectionMatrix; };
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
    static QnOpenGLRenderer& QnOpenGLRendererManager::instance(const QGLContext* a_context);

    QHash<const QGLContext*,QnOpenGLRenderer>& getContainer(){ return m_container; };
private:
    QHash<const QGLContext*,QnOpenGLRenderer> m_container;
};




#endif // QN_SIMPLE_SHADER_PROGRAM_H