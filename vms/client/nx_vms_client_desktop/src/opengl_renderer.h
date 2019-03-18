#pragma once

#include <nx/utils/singleton.h>

#include <QtCore/QScopedPointer>
#include <QtCore/QStack>
#include <QtGui/QMatrix4x4>

#include <QtGui/QOpenGLFunctions>

#include <QtOpenGL/QGLShaderProgram>

class QOpenGLWidget;
class QOpenGLVertexArrayObject;

class QnGLShaderProgram;
class QnColorGLShaderProgram;
class QnColorPerVertexGLShaderProgram;
class QnTextureGLShaderProgram;
class QnTextureTransitionShaderProgram;

class QnOpenGLRenderer: public QOpenGLFunctions
{

public:
    QnOpenGLRenderer(QObject *parent = NULL);

    void                setColor(const QVector4D& c);
    void                setColor(const QColor& c);

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
    QnTextureTransitionShaderProgram* getTextureTransitionShader() const;

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
    QScopedPointer<QnTextureTransitionShaderProgram> m_textureTransitionShader;
};

class QnOpenGLRendererManager: public QObject
{
    Q_OBJECT

public:
    QnOpenGLRendererManager(QObject* parent = nullptr);
    ~QnOpenGLRendererManager();

    static QnOpenGLRenderer* instance(QOpenGLWidget* widget);

private:
    QHash<QOpenGLWidget*, QnOpenGLRenderer*> m_container;
};

void loadImageData(
    int texture_wigth,
    int texture_height,
    int image_width,
    int image_heigth,
    int gl_bytes_per_pixel,
    int gl_format,
    const uchar* pixels);
