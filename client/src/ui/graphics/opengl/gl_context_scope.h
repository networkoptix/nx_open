#ifndef QN_GL_CONTEXT_SCOPE_H
#define QN_GL_CONTEXT_SCOPE_H

#include <QtOpenGL/QGLContext>

/**
 * This class temporarily makes a context current if not already current or
 * shared with the current context.  The previous context is made
 * current when the object goes out of scope.
 */
class QnGlContextScope {
public:
    QnGlContextScope(const QGLContext *ctx): 
        m_oldContext(NULL) 
    {
        QGLContext *currentContext = const_cast<QGLContext *>(QGLContext::currentContext());
        if (currentContext != ctx && !QGLContext::areSharing(ctx, currentContext)) {
            m_oldContext = currentContext;
            m_ctx = const_cast<QGLContext *>(ctx);
            m_ctx->makeCurrent();
        } else {
            m_ctx = currentContext;
        }
    }

    operator QGLContext *() const {
        return m_ctx;
    }

    QGLContext *operator->() const {
        return m_ctx;
    }

    ~QGLShareContextScope() {
        if (m_oldContext)
            m_oldContext->makeCurrent();
    }

private:
    QGLContext *m_oldContext;
    QGLContext *m_ctx;
};


#endif // QN_GL_CONTEXT_SCOPE_H
