// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "texture_color_shader_program.h"

class QnTextureTransitionShaderProgram: public QnTextureGLShaderProgram
{
    Q_OBJECT

public:
    QnTextureTransitionShaderProgram(QObject* parent = nullptr);

    void setTexture1(int target)
    {
        setUniformValue(m_texture1, target);
    }

    void setProgress(GLfloat progress)
    {
        setUniformValue(m_progressLocation, progress);
    }

    virtual bool compile() override;

    virtual bool link() override
    {
        bool result = QnTextureGLShaderProgram::link();
        if (result)
        {
            m_texture1 = uniformLocation("uTexture1");
            m_progressLocation = uniformLocation("aProgress");
        }
        return result;
    }

private:
    int m_texture1;
    int m_progressLocation;
};
