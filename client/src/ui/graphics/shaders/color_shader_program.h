#ifndef QN_COLOR_SHADER_PROGRAM_H
#define QN_COLOR_SHADER_PROGRAM_H

#include <QtCore/QSharedPointer>
#include <QtOpenGL/QGLShaderProgram>
#include "base_shader_program.h"

class QnColorGLShaderProgram : public QnGLShaderProgram 
{
    Q_OBJECT
public:
    QnColorGLShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);

    void setColor(const QVector4D& a_vec) {
        setUniformValue(m_color, a_vec);
    }

    virtual bool compile();

    virtual bool link() override
    {
        bool rez = QnGLShaderProgram::link();
        if (rez) {
            m_color = uniformLocation("uColor");
        }
        return rez;
    }

private:
    int m_color;
};


#endif // QN_COLOR_SHADER_PROGRAM_H
