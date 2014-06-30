#ifndef QN_PER_VERTEX_COLORED_SHADER_PROGRAM_H
#define QN_PER_VERTEX_COLORED_SHADER_PROGRAM_H



#include "color_shader_program.h"

class QnPerVertexColoredGLShaderProgramm : public QnColorGLShaderProgramm 
{
    Q_OBJECT
public:
    QnPerVertexColoredGLShaderProgramm(const QGLContext *context = NULL, QObject *parent = NULL);

    virtual bool compile();
};

#endif

