// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "blur_shader_program.h"

#include <nx/vms/client/core/graphics/shader_helper.h>

QnBlurShaderProgram::QnBlurShaderProgram(QObject *parent):
    base_type(parent)
{
    addShaderFromSourceCode(QOpenGLShader::Vertex, QN_SHADER_SOURCE(
        attribute vec2 aTexCoord;
        attribute vec4 aPosition;
        uniform mat4 uModelViewProjectionMatrix;
        varying vec2 vTexCoord;

        void main() {
            gl_Position = uModelViewProjectionMatrix * aPosition;
            vTexCoord = aTexCoord;
        }
    ));


    QByteArray shader(QN_SHADER_SOURCE(
        varying vec2 vTexCoord;
        uniform sampler2D uTexture;
        uniform sampler2D uMask;

        uniform vec2 texOffset; //< The inverse of the texture dimensions along X and Y
        uniform int horizontalPass; //< 0 or 1 to indicate vertical or horizontal pass

        void main()
        {
            if (texture2D(uMask, vTexCoord).a < 0.5) //< 0.0 or 1.0 is expected.
            {
                gl_FragColor = texture2D(uTexture, vTexCoord);
                return;
            }

            vec4 color = vec4(0.0);
            vec2 direction = 0 < horizontalPass ? vec2(1.0, 0.0) : vec2(0.0, 1.0);

            vec2 off1 = vec2(1.411764705882353) * direction;
            vec2 off2 = vec2(3.2941176470588234) * direction;
            vec2 off3 = vec2(5.176470588235294) * direction;

            color += texture2D(uTexture, vTexCoord) * 0.1964825501511404;
            color += texture2D(uTexture, vTexCoord + (off1 * texOffset)) * 0.2969069646728344;
            color += texture2D(uTexture, vTexCoord - (off1 * texOffset)) * 0.2969069646728344;
            color += texture2D(uTexture, vTexCoord + (off2 * texOffset)) * 0.09447039785044732;
            color += texture2D(uTexture, vTexCoord - (off2 * texOffset)) * 0.09447039785044732;
            color += texture2D(uTexture, vTexCoord + (off3 * texOffset)) * 0.010381362401148057;
            color += texture2D(uTexture, vTexCoord - (off3 * texOffset)) * 0.010381362401148057;

            gl_FragColor = color;
        }));

#ifdef QT_OPENGL_ES_2
    shader = QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QOpenGLShader::Fragment, shader);

    link();

}
