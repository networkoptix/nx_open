// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_PER_VERTEX_COLORED_SHADER_PROGRAM_H
#define QN_PER_VERTEX_COLORED_SHADER_PROGRAM_H

#include "color_shader_program.h"

class QnColorPerVertexGLShaderProgram : public QnColorGLShaderProgram
{
    Q_OBJECT
public:
    QnColorPerVertexGLShaderProgram(QObject *parent = nullptr);

    virtual bool compile();
};

#endif
