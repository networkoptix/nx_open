#ifndef QN_ARB_SHADER_H
#define QN_ARB_SHADER_H

#include <QObject>
#include <QScopedPointer>
#include <QGLShader>

class QnArbShaderProgramSharedData;

class QnArbShaderProgram: public QObject {
    Q_OBJECT;
public:
    QnArbShaderProgram(QObject *parent = NULL);

    virtual ~QnArbShaderProgram();

    bool bind();

    void release();

    bool addShaderFromSourceCode(QGLShader::ShaderType type, const char *source);

    void setLocalValue(QGLShader::ShaderType type, int location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

    static bool hasArbShaderPrograms(const QGLContext *context = NULL);

private:
    bool m_valid;
    GLuint m_fragmentProgram;
    const QnArbShaderProgramSharedData *m_shared;
};

#endif // QN_ARB_SHADER_H
