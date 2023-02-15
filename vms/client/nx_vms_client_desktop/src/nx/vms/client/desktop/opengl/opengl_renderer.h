// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/singleton.h>

#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QStack>
#include <QtGui/QMatrix4x4>

#include <QtGui/QOpenGLFunctions>

class QOpenGLWidget;
class QOpenGLVertexArrayObject;

class QnGLShaderProgram;
class QnColorGLShaderProgram;
class QnColorPerVertexGLShaderProgram;
class QnTextureGLShaderProgram;
class QnTextureTransitionShaderProgram;
class QnColorLineGLShaderProgram;
class QnBlurShaderProgram;

class QnOpenGLRenderer: public QOpenGLFunctions
{

public:
    QnOpenGLRenderer(QObject *parent = nullptr);
    virtual ~QnOpenGLRenderer();

    void                setColor(const QVector4D& c);
    void                setColor(const QColor& c);

    void                drawColoredQuad(const QRectF &rect , QnColorGLShaderProgram* shader = nullptr);
    void                drawPerVertexColoredPolygon(unsigned int a_buffer , unsigned int a_vertices_size , unsigned int a_polygon_state = GL_TRIANGLE_FAN);
    void                drawColoredQuad(const float* v_array, QnColorGLShaderProgram* shader = nullptr);

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
    QnColorLineGLShaderProgram* getColorLineShader() const;
    QnBlurShaderProgram* getBlurShader() const;

private:
    Q_DISABLE_COPY(QnOpenGLRenderer);

    QMatrix4x4 m_modelViewMatrix;
    QStack<QMatrix4x4> m_modelViewMatrixStack;
    QMatrix4x4 m_projectionMatrix;
    QStack<QMatrix4x4> m_projectionMatrixStack;
    QVector4D  m_color;

    unsigned short m_indices_for_render_quads[6];
    GLuint m_elementBuffer;

    QScopedPointer<QnColorGLShaderProgram> m_colorProgram;
    QScopedPointer<QnTextureGLShaderProgram> m_textureColorProgram;
    QScopedPointer<QnColorPerVertexGLShaderProgram> m_colorPerVertexShader;
    QScopedPointer<QnTextureTransitionShaderProgram> m_textureTransitionShader;
    QScopedPointer<QnColorLineGLShaderProgram> m_colorLineShader;
    QScopedPointer<QnBlurShaderProgram> m_blurShader;
};

class QnOpenGLRendererManager: public QObject
{
    Q_OBJECT

public:
    QnOpenGLRendererManager(QObject* parent = nullptr);
    ~QnOpenGLRendererManager();

    using RendererPtr = QSharedPointer<QnOpenGLRenderer>;

    RendererPtr get(QOpenGLWidget* widget);
    static RendererPtr instance(QOpenGLWidget* widget);

private:
    QHash<QOpenGLWidget*, RendererPtr> m_container;
};

void loadImageData(
    QOpenGLFunctions* renderer,
    int texture_wigth,
    int texture_height,
    int image_width,
    int image_heigth,
    int gl_bytes_per_pixel,
    int gl_format,
    const uchar* pixels);
