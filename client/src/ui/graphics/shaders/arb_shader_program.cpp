#include "arb_shader_program.h"
#include <QtOpenGL>
#include <utils/common/warnings.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>

class QnArbShaderProgramPrivate: public QnGlFunctions {
public:
    QnArbShaderProgramPrivate(const QGLContext *context): 
        QnGlFunctions(context),
        valid(false),
        fragmentProgram(-1)
    {}

    virtual ~QnArbShaderProgramPrivate() {}

    bool valid;
    GLuint fragmentProgram;
};

QnArbShaderProgram::QnArbShaderProgram(const QGLContext *context, QObject *parent): 
    QObject(parent), 
    d(new QnArbShaderProgramPrivate(context))
{}

QnArbShaderProgram::~QnArbShaderProgram() {
    if(d->fragmentProgram != -1)
        d->glDeleteProgramsARB(1, &d->fragmentProgram);
}

bool QnArbShaderProgram::hasArbShaderPrograms(const QGLContext *context) {
    return QnGlFunctions(context).features() & QnGlFunctions::ArbPrograms;
}

bool QnArbShaderProgram::bind() {
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    if(d->fragmentProgram != -1)
        d->glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, d->fragmentProgram);
    return true;
}

void QnArbShaderProgram::release() {
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
}

bool QnArbShaderProgram::addShaderFromSourceCode(QGLShader::ShaderType type, const char *source) {
    if(type != QGLShader::Fragment) {
        qnWarning("Only fragment ARB shaders are supported.");
        return false;
    }

    if(d->fragmentProgram == -1)
        d->glGenProgramsARB(1, &d->fragmentProgram);

    d->glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, d->fragmentProgram);
    d->glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, qstrlen(source), source);
    return glCheckError("glProgramStringARB") == GL_NO_ERROR;
}

void QnArbShaderProgram::setLocalValue(QGLShader::ShaderType type, int location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    if(type != QGLShader::Fragment) {
        qnWarning("Only fragment ARB shaders are supported.");
        return;
    }

    d->glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, location, x, y, z, w);
}

