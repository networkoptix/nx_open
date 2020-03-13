#pragma once

#include <QtCore/QSharedPointer>
#include "base_shader_program.h"

/**
 * Fragment shader for drawing smooth solid color lines.
 */
class QnColorLineGLShaderProgram: public QnGLShaderProgram
{
    Q_OBJECT
    using base_type = QnGLShaderProgram;

public:
    static constexpr int kComponentPerVertex = 3;

    QnColorLineGLShaderProgram(QObject* parent = nullptr);

    void setColor(const QVector4D& vec)
    {
        setUniformValue(m_color, vec);
    }

    void setFeather(const qreal feather)
    {
        setUniformValue(m_feather, (GLfloat)feather);
    }

    virtual bool compile() override;

    virtual bool link() override
    {
        bool rez = QnGLShaderProgram::link();
        if (rez)
        {
            m_color = uniformLocation("uColor");
            m_feather = uniformLocation("feather");
        }
        return rez;
    }

    // Generate buffer with triangles data (x, y, t), where t is 1D texture coord.
    // Load with setAttributeBuffer(<location>, GL_DOUBLE, 0, 3).
    static std::vector<GLdouble> genVericesFromLineStrip(
        const QVector<QPointF>& points,
        bool closed,
        qreal lineWidth);

private:
    int m_color = 0;
    int m_feather = 1;
};
