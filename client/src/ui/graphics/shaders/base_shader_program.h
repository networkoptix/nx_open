#ifndef QN_BASE_SHADER_PROGRAM_H
#define QN_BASE_SHADER_PROGRAM_H

#include <QtOpenGL/QGLShaderProgram>
#include <QtGui/QOpenGLShaderProgram>

class QnGLShaderProgram : public QOpenGLShaderProgram 
{
    Q_OBJECT
public:
    void setModelViewProjectionMatrix(const QMatrix4x4& a_m);

    virtual bool compile(){ return false; };

    virtual bool link() override
    {
        bool rez = QOpenGLShaderProgram::link();
        if (rez) {
            m_modelViewProjection = uniformLocation("uModelViewProjectionMatrix");
        }
        return rez;
    }

    void markInitialized() { m_initialized = true; };
    bool initialized() const { return m_initialized; };

    void addShaderFromSourceCode(QGLShader::ShaderType type, const QString& source) {
        addShaderFromSourceCode(type, source.toLatin1());
    }

    void addShaderFromSourceCode(QGLShader::ShaderType type, const QByteArray& source) {
        switch (type) {
        case QGLShader::Vertex:
            QOpenGLShaderProgram::addShaderFromSourceCode(QOpenGLShader::Vertex, source);
            break;
        case QGLShader::Fragment:
            QOpenGLShaderProgram::addShaderFromSourceCode(QOpenGLShader::Fragment, source);
            break;
        default:
            assert(false);
        }
    }

protected:
    QnGLShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL)
        : QOpenGLShaderProgram(parent),
          m_initialized(false)
    {
    };
private:
    int m_modelViewProjection;
    int m_vertices;
    bool m_initialized;
};

#endif // QN_COLOR_SHADER_PROGRAM_H