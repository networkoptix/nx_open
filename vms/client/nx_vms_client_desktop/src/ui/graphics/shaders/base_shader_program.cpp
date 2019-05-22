#include "base_shader_program.h"

void QnGLShaderProgram::setModelViewProjectionMatrix(const QMatrix4x4& value)
{
    setUniformValue(m_modelViewProjection, value);
}
