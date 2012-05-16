#include "gl_functions.h"
#include <QMutex>
#include <utils/common/warnings.h>
#include "gl_context_data.h"

#ifndef APIENTRY
#   define APIENTRY
#endif

typedef void (APIENTRY *PFNProgramStringARB) (GLenum, GLenum, GLsizei, const GLvoid *);
typedef void (APIENTRY *PFNBindProgramARB) (GLenum, GLuint);
typedef void (APIENTRY *PFNDeleteProgramsARB) (GLsizei, const GLuint *);
typedef void (APIENTRY *PFNGenProgramsARB) (GLsizei, GLuint *);
typedef void (APIENTRY *PFNProgramLocalParameter4fARB) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);

typedef void (APIENTRY *PFNActiveTexture) (GLenum);

namespace QnGl {
    bool qn_warnOnInvalidCalls = false;

#define WARN()                                                                  \
        if(qn_warnOnInvalidCalls)                                               \
            qnWarning("This function is not supported for current OpenGL context.");

    void APIENTRY glProgramStringARB(GLenum, GLenum, GLsizei, const GLvoid *) { WARN(); }
    void APIENTRY glBindProgramARB(GLenum, GLuint) { WARN(); }
    void APIENTRY glDeleteProgramsARB(GLsizei, const GLuint *) { WARN(); }
    void APIENTRY glGenProgramsARB(GLsizei, GLuint *) { WARN(); }
    void APIENTRY glProgramLocalParameter4fARB(GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat) { WARN(); }

    void APIENTRY glActiveTexture(GLenum) { WARN(); }

#undef WARN
} // namespace QnGl


// -------------------------------------------------------------------------- //
// QnGlFunctionsPrivate
// -------------------------------------------------------------------------- //
class QnGlFunctionsPrivate {
public:
    QnGlFunctionsPrivate(const QGLContext *context): 
        m_context(context),
        m_features(0)
    {
        bool status;

        status = true;
        status &= resolve<PFNProgramStringARB>          ("glProgramStringARB",              &QnGl::glProgramStringARB,              &glProgramStringARB);
        status &= resolve<PFNBindProgramARB>            ("glBindProgramARB",                &QnGl::glBindProgramARB,                &glBindProgramARB);
        status &= resolve<PFNDeleteProgramsARB>         ("glDeleteProgramsARB",             &QnGl::glDeleteProgramsARB,             &glDeleteProgramsARB);
        status &= resolve<PFNGenProgramsARB>            ("glGenProgramsARB",                &QnGl::glGenProgramsARB,                &glGenProgramsARB);
        status &= resolve<PFNProgramLocalParameter4fARB>("glProgramLocalParameter4fARB",    &QnGl::glProgramLocalParameter4fARB,    &glProgramLocalParameter4fARB);

        if(status)
            m_features |= QnGlFunctions::ArbPrograms;

        QByteArray renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
		if (vendor.contains("Tungsten Graphics") && renderer.contains("Gallium 0.1, Poulsbo on EMGD"))
			m_features |= QnGlFunctions::ShadersVendorBadList; // shaders are declared but does not works

        status = true;
        status &= resolve<PFNActiveTexture>             ("glActiveTexture",                 &QnGl::glActiveTexture,                 &glActiveTexture);
        if(status)
            m_features |= QnGlFunctions::OpenGL1_3;
    }

    virtual ~QnGlFunctionsPrivate() {}

    QnGlFunctions::Features features() const {
        return m_features;
    }

public:
    PFNProgramStringARB glProgramStringARB;
    PFNBindProgramARB glBindProgramARB;
    PFNDeleteProgramsARB glDeleteProgramsARB;
    PFNGenProgramsARB glGenProgramsARB;
    PFNProgramLocalParameter4fARB glProgramLocalParameter4fARB;

    PFNActiveTexture glActiveTexture;

private:
    template<class Function>
    bool resolve(const char *name, const Function &defaultValue, Function *result) {
        if(!m_context) {
            *result = defaultValue;
            return false;
        }
            
        *result = (Function) m_context->getProcAddress(QLatin1String(name));
        if(*result)
            return true;

        *result = defaultValue;
        return false;
    }

private:
    const QGLContext *m_context;
    QnGlFunctions::Features m_features;
};

typedef QnGlContextData<QnGlFunctionsPrivate, QnGlContextDataForwardingFactory<QnGlFunctionsPrivate> > QnGlFunctionsPrivateStorage;
Q_GLOBAL_STATIC(QnGlFunctionsPrivateStorage, qn_glFunctionsPrivateStorage);


// -------------------------------------------------------------------------- //
// QnGlFunctions
// -------------------------------------------------------------------------- //
QnGlFunctions::QnGlFunctions(const QGLContext *context) {
    if(context == NULL)
        context = QGLContext::currentContext();

    QnGlFunctionsPrivateStorage *storage = qn_glFunctionsPrivateStorage();
    if(storage)
        d = qn_glFunctionsPrivateStorage()->get(context);
    if(d.isNull()) /* Application is being shut down. */
        d = QSharedPointer<QnGlFunctionsPrivate>(new QnGlFunctionsPrivate(NULL));
}

QnGlFunctions::~QnGlFunctions() {}

QnGlFunctions::Features QnGlFunctions::features() const {
    return d->features();
}

void QnGlFunctions::enableWarnings(bool enable) {
    QnGl::qn_warnOnInvalidCalls = enable;
}

bool QnGlFunctions::isWarningsEnabled() {
    return QnGl::qn_warnOnInvalidCalls;
}

void QnGlFunctions::glProgramStringARB(GLenum target, GLenum format, GLsizei len, const GLvoid *string) const {
    d->glProgramStringARB(target, format, len, string);
}

void QnGlFunctions::glBindProgramARB(GLenum target, GLuint program) const {
    d->glBindProgramARB(target, program);
}

void QnGlFunctions::glDeleteProgramsARB(GLsizei n, const GLuint *programs) const {
    d->glDeleteProgramsARB(n, programs);
}

void QnGlFunctions::glGenProgramsARB(GLsizei n, GLuint *programs) const {
    d->glGenProgramsARB(n, programs);
}

void QnGlFunctions::glProgramLocalParameter4fARB(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) const {
    d->glProgramLocalParameter4fARB(target, index, x, y, z, w);
}

void QnGlFunctions::glActiveTexture(GLenum texture) {
    d->glActiveTexture(texture);
}