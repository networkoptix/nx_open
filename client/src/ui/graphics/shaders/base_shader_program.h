#ifndef QN_BASE_SHADER_PROGRAM_H
#define QN_BASE_SHADER_PROGRAM_H

#include <QtOpenGL/QGLShaderProgram>

class QnGLShaderProgram : public QGLShaderProgram 
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

    void markInitialized() { m_initialized = true; };
    bool initialized() const { return m_initialized; };

protected:
    QnGLShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL)
        : QGLShaderProgram(context,parent),
          m_initialized(false)
    {
    };
private:
    int m_modelViewProjection;
    int m_vertices;
    bool m_initialized;
};

#endif // QN_COLOR_SHADER_PROGRAM_H