// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "color_shader_program.h"

class QnTextureGLShaderProgram: public QnColorGLShaderProgram
{
    Q_OBJECT

public:
    QnTextureGLShaderProgram(QObject* parent = nullptr);

    void setTexture(int target)
    {
        setUniformValue(m_texture, target);
    }

    virtual bool compile() override;

    virtual bool link() override
    {
        bool result = QnColorGLShaderProgram::link();

        if (result)
            m_texture = uniformLocation("uTexture");

        return result;
    }

private:
    int m_texture;
};
