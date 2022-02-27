// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "color_shader_program.h"

#include <ui/graphics/opengl/gl_context_data.h>
#include <nx/vms/client/core/graphics/shader_helper.h>


QnColorGLShaderProgram::QnColorGLShaderProgram(QObject *parent)
    : QnGLShaderProgram(parent)
{
}

bool QnColorGLShaderProgram::compile()
{
    addShaderFromSourceCode(QOpenGLShader::Vertex, QN_SHADER_SOURCE(
        uniform mat4 uModelViewProjectionMatrix;
        attribute vec4 aPosition;
        void main() {
            gl_Position = uModelViewProjectionMatrix * aPosition;
        }
    ));

    QByteArray shader(QN_SHADER_SOURCE(
        uniform vec4 uColor;
        void main() {
            gl_FragColor = uColor;
        }
    ));

#ifdef QT_OPENGL_ES_2
    shader = QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QOpenGLShader::Fragment, shader);

    return link();
};
