#pragma once

#include "base_shader_program.h"

//#define SLOW_BLUR_SHADER

class QnBlurShaderProgram: public QnGLShaderProgram
{
    using base_type = QnGLShaderProgram;
public:
    QnBlurShaderProgram(QObject *parent = NULL);

    void setTexture(int target) { setUniformValue(m_texture, target); }
    void setTextureOffset(const QVector2D& textOffset) {
        setUniformValue(m_texOffset, textOffset);
    }
    void setHorizontalPass(bool value) { setUniformValue(m_horizontalPass, value ? 1 : 0); }
#ifdef SLOW_BLUR_SHADER
    void setBlurSize(int value) { setUniformValue(m_blurSize, value);  }
    void setSigma(float value) { setUniformValue(m_sigma, value); }
#endif

    virtual bool link() override
    {
        bool rez = base_type::link();
        if (rez)
		{
            m_texture = uniformLocation("uTexture");
            m_texOffset = uniformLocation("texOffset");
            m_horizontalPass = uniformLocation("horizontalPass");
#ifdef SLOW_BLUR_SHADER
            m_blurSize = uniformLocation("blurSize");
            m_sigma = uniformLocation("sigma");
#endif
        }
        return rez;
    }

private:
    int m_texture;
    int m_texOffset;
    int m_horizontalPass;
#ifdef SLOW_BLUR_SHADER
    int m_blurSize;
    int m_sigma;
#endif
};
