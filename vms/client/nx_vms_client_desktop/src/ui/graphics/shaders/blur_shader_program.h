// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "base_shader_program.h"

class QnBlurShaderProgram: public QnGLShaderProgram
{
    using base_type = QnGLShaderProgram;
public:
    QnBlurShaderProgram(QObject *parent = nullptr);

    void setTexture(int target) { setUniformValue(m_texture, target); }
    void setTextureOffset(const QVector2D& textOffset) {
        setUniformValue(m_texOffset, textOffset);
    }
    void setHorizontalPass(bool value) { setUniformValue(m_horizontalPass, value ? 1 : 0); }

    virtual bool link() override
    {
        bool rez = base_type::link();
        if (rez)
        {
            m_texture = uniformLocation("uTexture");
            m_texOffset = uniformLocation("texOffset");
            m_horizontalPass = uniformLocation("horizontalPass");
        }
        return rez;
    }

private:
    int m_texture;
    int m_texOffset;
    int m_horizontalPass;
};
