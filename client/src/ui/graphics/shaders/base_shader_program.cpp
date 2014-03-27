#include "base_shader_program.h"

void QnAbstractBaseGLShaderProgramm::setModelViewProjectionMatrix(const QMatrix4x4& a_m)
{
    setUniformValue(m_modelViewProjection, a_m);
}