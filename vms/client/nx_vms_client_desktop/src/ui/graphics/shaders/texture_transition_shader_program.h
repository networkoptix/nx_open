#pragma once

#include "texture_color_shader_program.h"

class QnTextureTransitionShaderProgram: public QnTextureGLShaderProgram
{
    Q_OBJECT

public:
    QnTextureTransitionShaderProgram(QObject *parent = NULL);

    void setTexture1(int target) {
        setUniformValue(m_texture1, target);
    }

    void setProgress(GLfloat progress) {
        setUniformValue(m_progressLocation, progress);
    }
    virtual bool compile();

    virtual bool link() override
    {
        bool rez = QnTextureGLShaderProgram::link();
        if (rez) {
            m_texture1 = uniformLocation("uTexture1");
            m_progressLocation = uniformLocation("aProgress");
        }
        return rez;
    }

private:
    int m_texture1;
    int m_progressLocation;
};
