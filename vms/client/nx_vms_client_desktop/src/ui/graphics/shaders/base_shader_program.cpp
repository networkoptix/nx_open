// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_shader_program.h"

void QnGLShaderProgram::setModelViewProjectionMatrix(const QMatrix4x4& value)
{
    setUniformValue(m_modelViewProjection, value);
}
