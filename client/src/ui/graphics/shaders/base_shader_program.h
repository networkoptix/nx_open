#ifndef QN_BASE_SHADER_PROGRAM_H
#define QN_BASE_SHADER_PROGRAM_H

#include <QtOpenGL/QGLShaderProgram>

class QnAbstractBaseGLShaderProgramm : public QGLShaderProgram 
{
    Q_OBJECT
public:
    void setModelViewProjectionMatrix(const QMatrix4x4& a_m);

    virtual bool compile(){ return false; };

    virtual bool link() override
    {
        bool rez = QGLShaderProgram::link();
        if (rez) {
            m_modelViewProjection = uniformLocation("uModelViewProjectionMatrix");
        }
        return rez;
    }

protected:
    QnAbstractBaseGLShaderProgramm(const QGLContext *context = NULL, QObject *parent = NULL)
        : QGLShaderProgram(context,parent)
    {
    };
private:
    int m_modelViewProjection;
    int m_vertices;
};

#endif // QN_COLOR_SHADER_PROGRAM_H