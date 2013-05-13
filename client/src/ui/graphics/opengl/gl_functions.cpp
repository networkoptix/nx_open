#include "gl_functions.h"

#define GL_GLEXT_PROTOTYPES /* We want typedefs, not function declarations. */
#include <GL/glext.h> /* Pull in all non-standard OpenGL defines. */

#include <boost/preprocessor/stringize.hpp>

#include <QtCore/QMutex>

#include <utils/common/warnings.h>

#include "gl_context_data.h"


#ifndef APIENTRY
#   define APIENTRY
#endif

namespace {
    bool qn_warnOnInvalidCalls = false;

    enum {
        DefaultMaxTextureSize = 1024, /* A sensible default supported by most modern GPUs. */
#ifdef Q_OS_LINUX
        LinuxMaxTextureSize = 4096
#endif
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
    void APIENTRY glBufferSubData(GLenum, GLintptrARB, GLsizeiptrARB, const GLvoid *) { WARN(); }
    GLvoid* APIENTRY glMapBuffer(GLenum, GLenum) { WARN(); return NULL; }
    GLboolean APIENTRY glUnmapBuffer(GLenum) { WARN(); return false; }

    void APIENTRY glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *) { WARN(); }
    void APIENTRY glDisableVertexAttribArray(GLuint) { WARN(); }
    void APIENTRY glEnableVertexAttribArray(GLuint) { WARN(); }

    GLsync APIENTRY glFenceSync(GLenum, GLbitfield) { WARN(); return 0; }
    void APIENTRY glDeleteSync(GLsync) { WARN(); }
    void APIENTRY glWaitSync(GLsync, GLbitfield, GLuint64) { WARN(); }
    GLAPI GLenum APIENTRY glClientWaitSync(GLsync, GLbitfield, GLuint64) { WARN(); return 0; }

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

        GLint maxTextureSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
#ifdef Q_OS_LINUX
        maxTextureSize = qMin((int)maxTextureSize, (int)LinuxMaxTextureSize);
#endif
        m_maxTextureSize = maxTextureSize;
    }
    

private:
    QMutex m_mutex;
    bool m_initialized;
    GLint m_maxTextureSize;
};

Q_GLOBAL_STATIC(QnGlFunctionsGlobal, qn_glFunctionsGlobal)


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
#define RESOLVE(FUNCTION_TYPE, FUNCTION_NAME)                                   \
        status &= resolve<FUNCTION_TYPE>(BOOST_PP_STRINGIZE(FUNCTION_NAME), &QnGl::FUNCTION_NAME, &FUNCTION_NAME)

        status = true;
        RESOLVE(PFNGLPROGRAMSTRINGARBPROC,              glProgramStringARB);
        RESOLVE(PFNGLBINDPROGRAMARBPROC,                glBindProgramARB);
        RESOLVE(PFNGLDELETEPROGRAMSARBPROC,             glDeleteProgramsARB);
        RESOLVE(PFNGLGENPROGRAMSARBPROC,                glGenProgramsARB);
        RESOLVE(PFNGLPROGRAMLOCALPARAMETER4FARBPROC,    glProgramLocalParameter4fARB);
        if(status)
            m_features |= QnGlFunctions::ArbPrograms;

        status = true;
        RESOLVE(PFNGLACTIVETEXTUREPROC,                 glActiveTexture);
        if(status)
            m_features |= QnGlFunctions::OpenGL1_3;

        status = true;
        RESOLVE(PFNGLBINDBUFFERPROC,                    glBindBuffer);
        RESOLVE(PFNGLDELETEBUFFERSPROC,                 glDeleteBuffers);
        RESOLVE(PFNGLGENBUFFERSPROC,                    glGenBuffers);
        RESOLVE(PFNGLBUFFERDATAPROC,                    glBufferData);
        RESOLVE(PFNGLBUFFERSUBDATAPROC,                 glBufferSubData);
        RESOLVE(PFNGLMAPBUFFERPROC,                     glMapBuffer);
        RESOLVE(PFNGLUNMAPBUFFERPROC,                   glUnmapBuffer);
        if(status)
            m_features |= QnGlFunctions::OpenGL1_5;

        status = true;
        RESOLVE(PFNGLVERTEXATTRIBPOINTERPROC,           glVertexAttribPointer);
        RESOLVE(PFNGLDISABLEVERTEXATTRIBARRAYPROC,      glDisableVertexAttribArray);
        RESOLVE(PFNGLENABLEVERTEXATTRIBARRAYPROC,       glEnableVertexAttribArray);
        if(status)
            m_features |= QnGlFunctions::OpenGL2_0;

        status = true;
        RESOLVE(PFNGLFENCESYNCPROC,                     glFenceSync);
        RESOLVE(PFNGLDELETESYNCPROC,                    glDeleteSync);
        RESOLVE(PFNGLCLIENTWAITSYNCPROC,                glClientWaitSync);
        RESOLVE(PFNGLWAITSYNCPROC,                      glWaitSync);
        if(status)
            m_features |= QnGlFunctions::OpenGL3_2 | QnGlFunctions::ArbSync;

#undef RESOLVE

        QByteArray renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
        if (vendor.contains("Tungsten Graphics") && renderer.contains("Gallium 0.1, Poulsbo on EMGD"))
            m_features |= QnGlFunctions::ShadersBroken; /* Shaders are declared but don't work. */
        
#ifdef Q_OS_LINUX
        QDir atiProcDir(QString::fromAscii("/proc/ati"));
        if(atiProcDir.exists()) /* Checking for fglrx driver. */
            m_features |= QnGlFunctions::NoOpenGLFullScreen;
#endif
    }

    virtual ~QnGlFunctionsPrivate()
    {
    }

    QnGlFunctions::Features features() const {
        return m_features;
    }

    const QGLContext* context() const
    {
        return m_context;
    }

public:
    PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;
    PFNGLBINDPROGRAMARBPROC glBindProgramARB;
    PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB;
    PFNGLGENPROGRAMSARBPROC glGenProgramsARB;
    PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB;

    PFNGLACTIVETEXTUREPROC glActiveTexture;

    PFNGLBINDBUFFERPROC glBindBuffer;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers;
    PFNGLGENBUFFERSPROC glGenBuffers;
    PFNGLBUFFERDATAPROC glBufferData;
    PFNGLBUFFERSUBDATAPROC glBufferSubData;
    PFNGLMAPBUFFERPROC glMapBuffer;
    PFNGLUNMAPBUFFERPROC glUnmapBuffer;

    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;

    PFNGLFENCESYNCPROC glFenceSync;
    PFNGLDELETESYNCPROC glDeleteSync;
    PFNGLCLIENTWAITSYNCPROC glClientWaitSync;
    PFNGLWAITSYNCPROC glWaitSync;

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

const QGLContext* QnGlFunctions::context() const
{
    return d->context();
}

QnGlFunctions::Features QnGlFunctions::features() const {
    return d->features();
}

void QnGlFunctions::setWarningsEnabled(bool enable) {
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

void QnGlFunctions::glBufferSubData(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data) {
    d->glBufferSubData( target, offset, size, data );
}

GLvoid *QnGlFunctions::glMapBuffer(GLenum target, GLenum access) {
    return d->glMapBuffer(target, access);
}

GLboolean QnGlFunctions::glUnmapBuffer(GLenum target) {
    return d->glUnmapBuffer(target);
}

void QnGlFunctions::glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer) {
    d->glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void QnGlFunctions::glEnableVertexAttribArray(GLuint index) {
    d->glEnableVertexAttribArray(index);
}

void QnGlFunctions::glDisableVertexAttribArray(GLuint index) {
    d->glDisableVertexAttribArray(index);
}

GLsync QnGlFunctions::glFenceSync(GLenum condition, GLbitfield flags)
{
    return d->glFenceSync(condition, flags);
}

void QnGlFunctions::glDeleteSync(GLsync sync)
{
    d->glDeleteSync(sync);
}

void QnGlFunctions::glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    d->glWaitSync(sync, flags, timeout);
}

GLenum QnGlFunctions::glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    return d->glClientWaitSync(sync, flags, timeout);
}

#ifdef Q_OS_WIN
namespace {
    QnGlFunctions::Features estimateFeatures() {
        QnGlFunctions::Features result(0);

        DISPLAY_DEVICEW dd; 
        dd.cb = sizeof(dd); 
        DWORD dev = 0; 
        while (EnumDisplayDevices(0, dev, &dd, 0)) {
            if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
                QString v = QString::fromWCharArray(dd.DeviceString);
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
    Q_GLOBAL_STATIC_WITH_ARGS(QnGlFunctions::Features, qn_estimatedFeatures, (estimateFeatures()));
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

