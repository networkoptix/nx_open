#include "base_shader_program.h"


void QnGLShaderProgram::setModelViewProjectionMatrix(const QMatrix4x4& a_m)
{
    setUniformValue(m_modelViewProjection, a_m);
}
