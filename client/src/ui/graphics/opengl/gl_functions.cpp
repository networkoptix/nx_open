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

typedef void (APIENTRY *PFNBindBuffer) (GLenum, GLuint);
typedef void (APIENTRY *PFNDeleteBuffers) (GLsizei, const GLuint *);
typedef void (APIENTRY *PFNGenBuffers) (GLsizei, GLuint *);
typedef void (APIENTRY *PFNBufferData) (GLenum, GLsizeiptrARB, const GLvoid *, GLenum);

namespace {
    bool qn_warnOnInvalidCalls = false;

    enum {
        DefaultMaxTextureSize = 1024, /* A sensible default supported by most modern GPUs. */
    };

} // anonymous namespace

namespace QnGl {
#define WARN()                                                                  \
    {                                                                           \
        if(qn_warnOnInvalidCalls)                                               \
            qnWarning("This function is not supported for current OpenGL context."); \
    }

    void APIENTRY glProgramStringARB(GLenum, GLenum, GLsizei, const GLvoid *) { WARN(); }
    void APIENTRY glBindProgramARB(GLenum, GLuint) { WARN(); }
    void APIENTRY glDeleteProgramsARB(GLsizei, const GLuint *) { WARN(); }
    void APIENTRY glGenProgramsARB(GLsizei, GLuint *) { WARN(); }
    void APIENTRY glProgramLocalParameter4fARB(GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat) { WARN(); }

    void APIENTRY glActiveTexture(GLenum) { WARN(); }

    void APIENTRY glBindBuffer(GLenum, GLuint) { WARN(); }
    void APIENTRY glDeleteBuffers(GLsizei, const GLuint *) { WARN(); }
    void APIENTRY glGenBuffers(GLsizei, GLuint *) { WARN(); }
    void APIENTRY glBufferData(GLenum, GLsizeiptrARB, const GLvoid *, GLenum) { WARN(); }

#undef WARN

} // namespace QnGl


// -------------------------------------------------------------------------- //
// QnGlFunctionsGlobal
// -------------------------------------------------------------------------- //
/**
 * A global object that contains state that is shared between all OpenGL functions
 * instances.
 */
class QnGlFunctionsGlobal {
public:
    QnGlFunctionsGlobal(): 
        m_initialized(false),
        m_maxTextureSize(DefaultMaxTextureSize)  
    {}

    GLint maxTextureSize() const {
        return m_maxTextureSize;
    }

    void initialize(const QGLContext *context) {
        QMutexLocker locker(&m_mutex);
        if(m_initialized)
            return;

        if(!QGLContext::currentContext()) {
            if(!context) {
                qnWarning("No OpenGL context in scope, initialization failed.");
                return;
            }
            
            /* Nothing bad could come out of this call, so const_cast is OK. */
            const_cast<QGLContext *>(context)->makeCurrent();
        }

        m_initialized = true;
        locker.unlock();

        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
    }
    

private:
    QMutex m_mutex;
    bool m_initialized;
    GLint m_maxTextureSize;
};

Q_GLOBAL_STATIC(QnGlFunctionsGlobal, qn_glFunctionsGlobal);


// -------------------------------------------------------------------------- //
// QnGlFunctionsPrivate
// -------------------------------------------------------------------------- //
/**
 * A per-context object that contains OpenGL functions state.
 */
class QnGlFunctionsPrivate {
public:
    QnGlFunctionsPrivate(const QGLContext *context): 
        m_context(context),
        m_features(0)
    {
        qn_glFunctionsGlobal()->initialize(context);

        bool status;
#define RESOLVE(FUNCTION)                                                       \
        status &= resolve<QN_CAT(PFN, FUNCTION)>("gl" QN_STRINGIZE(FUNCTION), &QnGl::QN_CAT(gl, FUNCTION), &QN_CAT(gl, FUNCTION))

        status = true;
        RESOLVE(ProgramStringARB);
        RESOLVE(BindProgramARB);
        RESOLVE(DeleteProgramsARB);
        RESOLVE(GenProgramsARB);
        RESOLVE(ProgramLocalParameter4fARB);
        if(status)
            m_features |= QnGlFunctions::ArbPrograms;

        status = true;
        RESOLVE(ActiveTexture);
        if(status)
            m_features |= QnGlFunctions::OpenGL1_3;

        status = true;
        RESOLVE(BindBuffer);
        RESOLVE(DeleteBuffers);
        RESOLVE(GenBuffers);
        RESOLVE(BufferData);
        if(status)
            m_features |= QnGlFunctions::OpenGL1_5;
#undef RESOLVE

        QByteArray renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
        if (vendor.contains("Tungsten Graphics") && renderer.contains("Gallium 0.1, Poulsbo on EMGD"))
            m_features |= QnGlFunctions::ShadersBroken; /* Shaders are declared but don't work. */
        if (vendor.contains("Intel") && renderer.contains("Intel(R) HD Graphics 3000"))
            m_features |= QnGlFunctions::OpenGLBroken; /* OpenGL is declared but don't work. */
        
#ifdef Q_OS_LINUX
        QDir atiProcDir(QString::fromAscii("/proc/ati"));
        if(atiProcDir.exists()) /* Checking for fglrx driver. */
            m_features |= QnGlFunctions::NoOpenGLFullScreen;
#endif
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

    PFNBindBuffer glBindBuffer;
    PFNDeleteBuffers glDeleteBuffers;
    PFNGenBuffers glGenBuffers;
    PFNBufferData glBufferData;

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
    qn_warnOnInvalidCalls = enable;
}

bool QnGlFunctions::isWarningsEnabled() {
    return qn_warnOnInvalidCalls;
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

void QnGlFunctions::glGenBuffers(GLsizei n, GLuint *buffers) {
    d->glGenBuffers(n, buffers);
}

void QnGlFunctions::glBindBuffer(GLenum target, GLuint buffer) {
    d->glBindBuffer(target, buffer);
}

void QnGlFunctions::glDeleteBuffers(GLsizei n, const GLuint *buffers) {
    d->glDeleteBuffers(n, buffers);
}

void QnGlFunctions::glBufferData(GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage) {
    d->glBufferData(target, size, data, usage);
}

#ifdef Q_OS_WIN
namespace {
    QnGlFunctions::Features estimateFeatures() {
        QnGlFunctions::Features result(0);

        DISPLAY_DEVICE dd; 
        dd.cb = sizeof(dd); 
        DWORD dev = 0; 
        while (EnumDisplayDevices(0, dev, &dd, 0))
        {
            if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE){
                QString v = QString::fromWCharArray(dd.DeviceString);
                if (v.contains(QLatin1String("Intel(R) HD Graphics 3000")))
                    result |= QnGlFunctions::OpenGLBroken;
                else
                if (v.contains(QLatin1String("Gallium 0.1, Poulsbo on EMGD")))
                    result |= QnGlFunctions::ShadersBroken;
                break;
            }
            ZeroMemory(&dd, sizeof(dd));
            dd.cb = sizeof(dd);
            dev++; 
        }
        return result;
    }
    Q_GLOBAL_STATIC_WITH_INITIALIZER(QnGlFunctions::Features, qn_estimatedFeatures, { *x = estimateFeatures(); });
}
#endif

QnGlFunctions::Features QnGlFunctions::estimatedFeatures() {
#ifdef Q_OS_WIN
    return *qn_estimatedFeatures();
#else
    return QnGlFunctions::Features(0);
#endif
}

GLint QnGlFunctions::estimatedInteger(GLenum target) {
    if(target != GL_MAX_TEXTURE_SIZE) {
        qnWarning("Target '%1' is not supported.", target);
        return 0;
    }

    QnGlFunctionsGlobal *global = qn_glFunctionsGlobal();
    if(global) {
        return global->maxTextureSize();
    } else {
        /* We may get called when application is shutting down. */
        return DefaultMaxTextureSize;
    }
}

