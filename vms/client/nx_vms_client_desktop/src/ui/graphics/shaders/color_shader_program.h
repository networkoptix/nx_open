// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>
#include "base_shader_program.h"

class QnColorGLShaderProgram: public QnGLShaderProgram
{
    Q_OBJECT

public:
    QnColorGLShaderProgram(QObject *parent = nullptr);

    void setColor(const QVector4D& a_vec)
    {
        setUniformValue(m_color, a_vec);
    }

    virtual bool compile() override;

    virtual bool link() override
    {
        bool result = QnGLShaderProgram::link();

        if (result)
            m_color = uniformLocation("uColor");

        return result;
    }

private:
    int m_color;
};
