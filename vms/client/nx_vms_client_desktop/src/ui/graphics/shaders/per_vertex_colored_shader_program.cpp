// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "per_vertex_colored_shader_program.h"
#include <nx/vms/client/core/graphics/shader_helper.h>

QnColorPerVertexGLShaderProgram::QnColorPerVertexGLShaderProgram(QObject *parent)
    : QnColorGLShaderProgram(parent)
{
}
bool QnColorPerVertexGLShaderProgram::compile()
{
    addShaderFromSourceCode(QOpenGLShader::Vertex, QN_SHADER_SOURCE(
    uniform lowp mat4 uModelViewProjectionMatrix;
    attribute lowp vec4 aPosition;
    attribute lowp vec4 aColor;
    varying lowp vec4 vColor;

    void main() {
        gl_Position = uModelViewProjectionMatrix * aPosition;
        vColor = aColor;
    }
    ));

     QByteArray shader(QN_SHADER_SOURCE(
     uniform lowp vec4 uColor;
     varying lowp vec4 vColor;
     void main() {
         gl_FragColor = vColor*uColor;
     }
     ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QOpenGLShader::Fragment, shader);

    return link();
};
