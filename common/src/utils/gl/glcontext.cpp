/**********************************************************
* 14 oct 2012
* a.kolesnikov
***********************************************************/

#include "glcontext.h"

#include <QGLContext>


//!Creates context shared with \a contextToShareWith (if not NULL)
//GLContext::GLContext( SYS_GL_CTX_HANDLE contextToShareWith )
//:
//    m_handle( NULL )
//{
//    m_handle = wglCreateContext();
//    if( m_handle == NULL )
//    {
//        //TODO/IMPL get error description
//    }
//}

GLContext::GLContext( WId wnd, SYS_GL_CTX_HANDLE contextHandleToShareWith )
:
    m_handle( NULL ),
    m_dc( NULL ),
    m_winID( wnd ),
    m_previousErrorCode( 0 )
{
    m_dc = GetDC( m_winID );
    m_handle = wglCreateContext( m_dc );
    ReleaseDC( m_winID, m_dc );
    m_dc = NULL;
    if( m_handle == NULL )
    {
        m_previousErrorCode = GetLastError();
        return;
    }

    if( contextHandleToShareWith == NULL )
        return;

    if( !wglShareLists( contextHandleToShareWith, m_handle ) )
    {
        m_previousErrorCode = GetLastError();
        wglDeleteContext( m_handle );
        m_handle = NULL;
    }
}

GLContext::~GLContext()
{
    if( m_handle != NULL )
    {
        if( wglDeleteContext( m_handle ) )
            m_handle = NULL;
    }

    if( m_dc != NULL )
    {
        DeleteDC( m_dc );
    }
}

bool GLContext::makeCurrent( SYS_PAINT_DEVICE_HANDLE paintDevToUse )
{
    m_dc = paintDevToUse != NULL ? paintDevToUse : GetDC( m_winID );
    BOOL res = wglMakeCurrent( m_dc, m_handle );
    m_previousErrorCode = GetLastError();
    if( paintDevToUse != NULL )
        m_dc = NULL;    //no need to release device context at doneCurrent
    if( !res )
        int x = 0;
    return res;
}

void GLContext::doneCurrent()
{
    wglMakeCurrent( NULL, NULL );
    if( m_dc != NULL )
        ReleaseDC( m_winID, m_dc );
    m_dc = NULL;
}

GLContext::SYS_GL_CTX_HANDLE GLContext::handle() const
{
    return m_handle;
}

bool GLContext::isValid() const
{
    return m_handle != NULL;
}

QString GLContext::getLastErrorString() const
{
    //TODO/IMPL get error description
    return QString();
}

//void* GLContext::getProcAddress( const char* procName ) const
//{
//    //TODO/IMPL
//    return NULL;
//}

bool GLContext::shareWith( SYS_GL_CTX_HANDLE ctxID )
{
    if( !wglShareLists( ctxID, m_handle ) )
    {
        m_previousErrorCode = GetLastError();
        return false;
    }

    return true;
}

//!Returns system-dependent handle of qt context \a ctx
/*!
    This method can be relied on as long as \a QGLContext is not thread-safe
    \note This method can be called from GUI thread only
*/
GLContext::SYS_GL_CTX_HANDLE GLContext::getSysHandleOfQtContext( const QGLContext* context, SYS_PAINT_DEVICE_HANDLE* const paintDeviceHandle )
{
    const QGLContext* currentCtx = QGLContext::currentContext();
    if( currentCtx != context )
    {
        if( currentCtx )
            const_cast<QGLContext*>(currentCtx)->doneCurrent();
        const_cast<QGLContext*>(context)->makeCurrent();
    }

#ifdef _WIN32
    SYS_GL_CTX_HANDLE sysContextHandle = wglGetCurrentContext();
#else
    SYS_GL_CTX_HANDLE sysContextHandle = glXGetCurrentContext();
#endif

    if( paintDeviceHandle )
    {
#ifdef _WIN32
        //QPaintDevice* contextDevice = context->device();
        //if( contextDevice )
        //{
        //    QWidget* deviceAsWidget = dynamic_cast<QWidget*>(contextDevice);
        //    if( deviceAsWidget )
        //    {
        //        if( paintDeviceHandle )
        //            *paintDeviceHandle = deviceAsWidget->getDC();
        //    }
        //}

        if( paintDeviceHandle && *paintDeviceHandle == NULL )
            *paintDeviceHandle = wglGetCurrentDC();
#else
#error "Not implemented"
#endif
    }

    if( currentCtx != context )
    {
        const_cast<QGLContext*>(context)->doneCurrent();
        if( currentCtx )
            const_cast<QGLContext*>(currentCtx)->makeCurrent();
    }

    return sysContextHandle;
}
