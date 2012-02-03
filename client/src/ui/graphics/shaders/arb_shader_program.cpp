#include "arb_shader_program.h"
#include <QtOpenGL>
#include <utils/common/warnings.h>
#include <ui/common/opengl.h>

#ifndef GL_FRAGMENT_PROGRAM_ARB
#   define GL_FRAGMENT_PROGRAM_ARB           0x8804
#   define GL_PROGRAM_FORMAT_ASCII_ARB       0x8875
#endif

#ifndef APIENTRY
#   define APIENTRY
#endif

namespace {
    typedef void (APIENTRY *PFNProgramStringARB) (GLenum, GLenum, GLsizei, const GLvoid *);
    typedef void (APIENTRY *PFNBindProgramARB) (GLenum, GLuint);
    typedef void (APIENTRY *PFNDeleteProgramsARB) (GLsizei, const GLuint *);
    typedef void (APIENTRY *PFNGenProgramsARB) (GLsizei, GLuint *);
    typedef void (APIENTRY *PFNProgramLocalParameter4fARB) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
} // anonymous namespace

class QnArbShaderProgramSharedData {
public:
    QnArbShaderProgramSharedData(): 
        valid(false) 
    {
        const QGLContext *ctx = QGLContext::currentContext();
        if(!ctx)
            return;

        glProgramStringARB              = (PFNProgramStringARB)             QGLContext::currentContext()->getProcAddress(QLatin1String("glProgramStringARB"));
        glBindProgramARB                = (PFNBindProgramARB)               QGLContext::currentContext()->getProcAddress(QLatin1String("glBindProgramARB"));
        glDeleteProgramsARB             = (PFNDeleteProgramsARB)            QGLContext::currentContext()->getProcAddress(QLatin1String("glDeleteProgramsARB"));
        glGenProgramsARB                = (PFNGenProgramsARB)               QGLContext::currentContext()->getProcAddress(QLatin1String("glGenProgramsARB"));
        glProgramLocalParameter4fARB    = (PFNProgramLocalParameter4fARB)   QGLContext::currentContext()->getProcAddress(QLatin1String("glProgramLocalParameter4fARB"));
        if(!glProgramStringARB || !glBindProgramARB || !glDeleteProgramsARB || !glGenProgramsARB || !glProgramLocalParameter4fARB)
            return;

        valid = true;
    }

public:
    bool valid;

    PFNProgramStringARB glProgramStringARB;
    PFNBindProgramARB glBindProgramARB;
    PFNDeleteProgramsARB glDeleteProgramsARB;
    PFNGenProgramsARB glGenProgramsARB;
    PFNProgramLocalParameter4fARB glProgramLocalParameter4fARB;
};

Q_GLOBAL_STATIC(QnArbShaderProgramSharedData, qn_arbShaderProgramSharedData);

QnArbShaderProgram::QnArbShaderProgram(QObject *parent): 
    QObject(parent), 
    m_valid(false),
    m_fragmentProgram(-1),
    m_shared(qn_arbShaderProgramSharedData())
{
    m_valid = m_shared->valid;
}

QnArbShaderProgram::~QnArbShaderProgram() {
    if(m_fragmentProgram != -1)
        m_shared->glDeleteProgramsARB(1, &m_fragmentProgram);
}

bool QnArbShaderProgram::hasArbShaderPrograms(const QGLContext *context) {
    Q_UNUSED(context);

    /* Context is unused for now. */
    return qn_arbShaderProgramSharedData()->valid;
}

bool QnArbShaderProgram::bind() {
    if(!m_valid)
        return false;

    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    if(m_fragmentProgram != -1)
        m_shared->glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_fragmentProgram);
    return true;
}

void QnArbShaderProgram::release() {
    if(!m_valid)
        return;

    glDisable(GL_FRAGMENT_PROGRAM_ARB);
}

bool QnArbShaderProgram::addShaderFromSourceCode(QGLShader::ShaderType type, const char *source) {
    if(!m_valid)
        return false;

    if(type != QGLShader::Fragment) {
        qnWarning("Only fragment ARB shaders are supported.");
        return false;
    }

    if(m_fragmentProgram == -1)
        m_shared->glGenProgramsARB(1, &m_fragmentProgram);

    m_shared->glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_fragmentProgram);
    m_shared->glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, qstrlen(source), source);
    return glCheckError("glProgramStringARB") != GL_NO_ERROR;
}

void QnArbShaderProgram::setLocalValue(QGLShader::ShaderType type, int location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    if(!m_valid)
        return;

    if(type != QGLShader::Fragment) {
        qnWarning("Only fragment ARB shaders are supported.");
        return;
    }

    m_shared->glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, location, x, y, z, w);
}

