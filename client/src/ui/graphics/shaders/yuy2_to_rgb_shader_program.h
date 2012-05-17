#ifndef QN_YUY2_TO_RGB_SHADER_PROGRAM_H
#define QN_YUY2_TO_RGB_SHADER_PROGRAM_H

#include "arb_shader_program.h"

class QnYuy2ToRgbShaderProgram: public QnArbShaderProgram {
    Q_OBJECT;
public:
    QnYuy2ToRgbShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);
};

#endif // QN_YUY2_TO_RGB_SHADER_PROGRAM_H
