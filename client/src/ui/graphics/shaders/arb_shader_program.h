#ifndef QN_ARB_SHADER_H
#define QN_ARB_SHADER_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtOpenGL/QGLShader>

class QnArbShaderProgramPrivate;

class QnArbShaderProgram: public QObject {
    Q_OBJECT;
public:
    QnArbShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);

    virtual ~QnArbShaderProgram();

    bool bind();

    void release();

    bool isValid() const;

    bool addShaderFromSourceCode(QGLShader::ShaderType type, const char *source);

    void setLocalValue(QGLShader::ShaderType type, int location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

    static bool hasArbShaderPrograms(const QGLContext *context = NULL);

private:
    QScopedPointer<QnArbShaderProgramPrivate> d;
};

#endif // QN_ARB_SHADER_H
