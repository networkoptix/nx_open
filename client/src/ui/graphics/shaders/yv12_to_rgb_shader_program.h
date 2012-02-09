#ifndef QN_YV12_TO_RGB_SHADER_PROGRAM_H
#define QN_YV12_TO_RGB_SHADER_PROGRAM_H

#include "arb_shader_program.h"

class QnYv12ToRgbShaderProgram: public QnArbShaderProgram {
    Q_OBJECT;
public:
    QnYv12ToRgbShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);

    void setParameters(GLfloat brightness, GLfloat contrast, GLfloat hue, GLfloat saturation, GLfloat opacity);
};

#endif // QN_YV12_TO_RGB_SHADER_PROGRAM_H
