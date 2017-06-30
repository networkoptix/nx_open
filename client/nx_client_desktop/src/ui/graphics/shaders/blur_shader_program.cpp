#include "blur_shader_program.h"

#include "shader_source.h"

QnBlurShaderProgram::QnBlurShaderProgram(QObject *parent) :
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

#ifdef SLOW_BLUR_SHADER
    QByteArray shader(QN_SHADER_SOURCE(
        varying vec2 vTexCoord;
        uniform sampler2D uTexture;

        // The inverse of the texture dimensions along X and Y
        uniform vec2 texOffset;

        uniform int blurSize;
        uniform int horizontalPass; // 0 or 1 to indicate vertical or horizontal pass

        uniform float sigma;        // The sigma value for the gaussian function: higher value means more blur
                                    // A good value for 9x9 is around 3 to 5
                                    // A good value for 7x7 is around 2.5 to 4
                                    // A good value for 5x5 is around 2 to 3.5
                                    // ... play around with this based on what you need <span class="Emoticon Emoticon1"><span>:)</span></span>

        const float pi = 3.14159265;

        void main()
        {
            float numBlurPixelsPerSide = float(blurSize / 2);

            vec2 blurMultiplyVec = 0 < horizontalPass ? vec2(1.0, 0.0) : vec2(0.0, 1.0);

            // Incremental Gaussian Coefficent Calculation (See GPU Gems 3 pp. 877 - 889)
            vec3 incrementalGaussian;
            incrementalGaussian.x = 1.0 / (sqrt(2.0 * pi) * sigma);
            incrementalGaussian.y = exp(-0.5 / (sigma * sigma));
            incrementalGaussian.z = incrementalGaussian.y * incrementalGaussian.y;

            vec4 avgValue = vec4(0.0, 0.0, 0.0, 0.0);
            float coefficientSum = 0.0;

            // Take the central sample first...
            avgValue += texture2D(uTexture, vTexCoord.st) * incrementalGaussian.x;
            coefficientSum += incrementalGaussian.x;
            incrementalGaussian.xy *= incrementalGaussian.yz;

            // Go through the remaining 8 vertical samples (4 on each side of the center)
            for (float i = 1.0; i <= numBlurPixelsPerSide; i++)
            {
                avgValue += texture2D(uTexture, vTexCoord.st - i * texOffset *
                    blurMultiplyVec) * incrementalGaussian.x;
                avgValue += texture2D(uTexture, vTexCoord.st + i * texOffset *
                    blurMultiplyVec) * incrementalGaussian.x;
                coefficientSum += 2.0 * incrementalGaussian.x;
                incrementalGaussian.xy *= incrementalGaussian.yz;
            }

            gl_FragColor = avgValue / coefficientSum;
        }
        ));
#else
    QByteArray shader(QN_SHADER_SOURCE(
        varying vec2 vTexCoord;
        uniform sampler2D uTexture;

        uniform vec2 texOffset; //< The inverse of the texture dimensions along X and Y
        uniform int horizontalPass; //< 0 or 1 to indicate vertical or horizontal pass

        void main()
        {
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
        }
        ));
#endif

#ifdef QT_OPENGL_ES_2
    shader = QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QOpenGLShader::Fragment, shader);

    link();

}
