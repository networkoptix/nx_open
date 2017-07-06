/**********************************************************
* 14 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef GLCONTEXT_H
#define GLCONTEXT_H

#if defined(_WIN32) || defined(Q_OS_WIN) || defined(Q_OS_LINUX)

#define NX_GLCONTEXT_PRESENT

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#ifdef Q_OS_WIN
#include <WinGDI.h>
#else
struct __GLXcontextRec;
#endif

class QGLContext;
class QGLWidget;


//!Cross-platform GL context which can be used in any thread (not in GUI only) unlike QGLContext
class GLContext
{
public:
#ifdef Q_OS_WIN
    typedef HGLRC SYS_GL_CTX_HANDLE;
    typedef HDC SYS_PAINT_DEVICE_HANDLE;
#else
    typedef __GLXcontextRec *SYS_GL_CTX_HANDLE;
    typedef void* SYS_PAINT_DEVICE_HANDLE;
#endif

    //!Calls \a makeCurrent() at instanciation, calls \a doneCurrent at destroy
    class ScopedContextUsage
    {
    public:
        ScopedContextUsage( GLContext* const glContext, SYS_PAINT_DEVICE_HANDLE paintDevToUse = NULL )
        :
            m_glContext( glContext ),
            m_isCurrent( false )
        {
            m_isCurrent = m_glContext->makeCurrent( paintDevToUse );
        }

        bool isCurrent() const
        {
            return m_isCurrent;
        }

        ~ScopedContextUsage()
        {
            m_glContext->doneCurrent();
        }

    private:
        GLContext* const m_glContext;
        bool m_isCurrent;
    };

    //!Creates context shared with \a contextToShareWith on paint device of \a contextToShareWith
    /*!
        One MUST call \a isValid just after object instanciation to check whether context has been created successfully.
        In case of error, call \a getLastErrorString() for error description
        \param contextHandleToShareWith MUST not be NULL
        \param System id of window to paint to. Window's device context is used to create gl context. If NULL, screen device context is used
        \note This method MUST be called from GUI thread only
    */
    GLContext( WId wnd = 0, SYS_GL_CTX_HANDLE contextHandleToShareWith = NULL );
    //!Create context, shared with \a shareWidget context 
    /*!
        \note This constructor can be called from GUI thread only, since it uses \a getSysHandleOfQtContext method
    */
    GLContext( const QGLWidget* shareWidget );
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
    //bool shareWith( SYS_GL_CTX_HANDLE ctxID );
    //!Returns window ID, gl context has been created with
    WId wnd() const;

    //!Returns system-dependent handle of qt context \a ctx
    /*!
        This method can be relied on as long as \a QGLContext is not thread-safe
        \param paintDeviceHandle if not NULL, will be filled with handle of paint device of context \a ctx
        \note This method can be called from GUI thread only
        \note This method can perform gl context switching, so it can be performance-heavy
    */
    static SYS_GL_CTX_HANDLE getSysHandleOfQtContext( const QGLContext* ctx, SYS_PAINT_DEVICE_HANDLE* const paintDeviceHandle = NULL );

private:
    SYS_GL_CTX_HANDLE m_handle;
    SYS_PAINT_DEVICE_HANDLE m_dc;
    std::unique_ptr<QWidget> m_widget;
    WId m_winID;
    unsigned int m_previousErrorCode;
#ifdef Q_OS_WIN
    PIXELFORMATDESCRIPTOR m_pfd;
#endif

    void initialize( WId wnd, SYS_GL_CTX_HANDLE contextHandleToShareWith );
};

#endif  //defined(Q_OS_WIN) || defined(Q_OS_LINUX)

#endif  //GLCONTEXT_H
