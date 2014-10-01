#ifndef QN_PER_VERTEX_COLORED_SHADER_PROGRAM_H
#define QN_PER_VERTEX_COLORED_SHADER_PROGRAM_H

#include "color_shader_program.h"

class QnColorPerVertexGLShaderProgram : public QnColorGLShaderProgram 
{
    Q_OBJECT
public:
    QnColorPerVertexGLShaderProgram(QObject *parent = NULL);

    virtual bool compile();
};

#endif

