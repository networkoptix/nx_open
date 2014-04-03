#ifdef QT_OPENGL_ES_2
#ifndef QN_ES_PER_VERTEX_COLORED_SHADER_PROGRAM_H
#define QN_ES_PER_VERTEX_COLORED_SHADER_PROGRAM_H



#include "es_color_shader_program.h"

class QnPerVertexColoredGLShaderProgramm : public QnColorGLShaderProgramm 
{
    Q_OBJECT
public:
    QnPerVertexColoredGLShaderProgramm(const QGLContext *context = NULL, QObject *parent = NULL);

    virtual bool compile();
};


#endif

#endif // QT_OPENGL_ES_2