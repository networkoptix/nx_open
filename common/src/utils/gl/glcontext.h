/**********************************************************
* 14 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef GLCONTEXT_H
#define GLCONTEXT_H

#ifdef _WIN32
#include <Wingdi.h>
#else
#include <GL/glx.h>
#endif
#include <QString>


class QGLContext;

//!Cross-platform GL context which can be used in any thread (not in GUI only) unlike QGLContext
class GLContext
{
public:
#ifdef _WIN32
    typedef HGLRC SYS_GL_CTX_HANDLE;
    typedef HDC SYS_PAINT_DEVICE_HANDLE;
#else
    typedef GLXContext SYS_GL_CTX_HANDLE;
    typedef void* SYS_PAINT_DEVICE_HANDLE;
#endif

    //!Creates context shared with \a contextToShareWith on paint device of \a contextToShareWith
    /*!
        One MUST call \a isValid just after object instanciation to check whether context has been created successfully.
        In case of error, call \a getLastErrorString() for error description
        \param contextHandleToShareWith MUST not be NULL
        \param System id of window to paint to. Window's device context is used to create gl context. If NULL, screen device context is used
        \note This method MUST be called from GUI thread only
    */
    GLContext( WId wnd = NULL, SYS_GL_CTX_HANDLE contextHandleToShareWith = NULL );
    ~GLContext();

    /*!
        \param devToUse If \a NULL, paint device of window, supplied at initialization, is used
    */
    bool makeCurrent( SYS_PAINT_DEVICE_HANDLE paintDevToUse = NULL );
    void doneCurrent();
    //!Returns system-dependent handle of GL context. If no context, NULL is returned
    SYS_GL_CTX_HANDLE handle() const;
    bool isValid() const;
    //!Returns previous command error text
    QString getLastErrorString() const;
    //void* getProcAddress( const char* procName ) const;
    bool shareWith( SYS_GL_CTX_HANDLE ctxID );

    //!Returns system-dependent handle of qt context \a ctx
    /*!
        This method can be relied on as long as \a QGLContext is not thread-safe
        \param paintDeviceHandle if not NULL, will be filled with handle of paint device of context \a ctx
        \note This method can be called from GUI thread only
    */
    static SYS_GL_CTX_HANDLE getSysHandleOfQtContext( const QGLContext* ctx, SYS_PAINT_DEVICE_HANDLE* const paintDeviceHandle = NULL );

private:
    SYS_GL_CTX_HANDLE m_handle;
    SYS_PAINT_DEVICE_HANDLE m_dc;
    WId m_winID;
    unsigned int m_previousErrorCode;
};

#endif  //GLCONTEXT_H
