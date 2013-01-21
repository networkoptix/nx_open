/**********************************************************
* 14 oct 2012
* a.kolesnikov
***********************************************************/

#include "glcontext.h"

#include <QGLContext>

#ifndef _WIN32
#include <QX11Info>

#include <GL/glx.h>
#include <X11/Xlib.h>
#endif

#include "../common/log.h"


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
{
    initialize( wnd, contextHandleToShareWith );
}

GLContext::GLContext( const QGLWidget* shareWidget )
{
    initialize(
        shareWidget->winId(),
        getSysHandleOfQtContext( shareWidget->context() ) );
}

GLContext::~GLContext()
{
#ifdef _WIN32
    Q_ASSERT( IsWindow(m_winID) );

    if( m_handle != NULL )
    {
        wglDeleteContext( m_handle );
        m_handle = NULL;
    }

    if( m_dc != NULL )
    {
        DeleteDC( m_dc );
        m_dc = NULL;
    }
#else
    if( m_handle )
    {
        glXDestroyContext( QX11Info::display(), m_handle );
        m_handle = NULL;
    }
#endif
}

bool GLContext::makeCurrent( SYS_PAINT_DEVICE_HANDLE paintDevToUse )
{
    //qDebug()<<"GLContext::makeCurrent. "<<m_handle;

    //return true;

#ifdef _WIN32
    if( paintDevToUse != NULL )
    {
        m_dc = paintDevToUse;
    }
    else
    {
        Q_ASSERT( IsWindow( m_winID ) );
        m_dc = GetDC( m_winID );
    }
    BOOL res = wglMakeCurrent( m_dc, m_handle );
    m_previousErrorCode = GetLastError();
    if( paintDevToUse != NULL )
        m_dc = NULL;    //no need to release device context at doneCurrent
    return res;
#else
    bool res = glXMakeCurrent(
        QX11Info::display(),
        QX11Info::appRootWindow(QX11Info::appScreen()),
        m_handle );
    m_previousErrorCode = glGetError();
    return res;
#endif
}

void GLContext::doneCurrent()
{
#ifdef _WIN32
    wglMakeCurrent( NULL, NULL );
    if( m_dc != NULL )
        ReleaseDC( m_winID, m_dc );
    m_dc = NULL;
#else
    glXMakeCurrent(
        QX11Info::display(),
        0L,
        NULL );
    m_previousErrorCode = glGetError();
#endif
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
/*
bool GLContext::shareWith( SYS_GL_CTX_HANDLE ctxID )
{
#ifdef _WIN32
    if( !wglShareLists( ctxID, m_handle ) )
    {
        m_previousErrorCode = GetLastError();
        return false;
    }

    return true;
#else
    return false;
#endif
}
*/
WId GLContext::wnd() const
{
    return m_winID;
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

void GLContext::initialize( WId wnd, SYS_GL_CTX_HANDLE contextHandleToShareWith )
{
    m_handle = NULL;
    m_dc = NULL;
    m_winID = wnd;
    m_previousErrorCode = 0;
#ifdef USE_INTERNAL_WIDGET
    m_widget.reset( new QWidget() );
#endif

#ifdef USE_INTERNAL_WIDGET
    m_winID = m_widget->winId();
#endif

#ifdef _WIN32
    Q_ASSERT( IsWindow( m_winID ) );

    HDC dc = GetDC( m_winID );
#ifdef USE_INTERNAL_WIDGET
    //reading pixel format of src window
    {
        HDC hdc = GetDC( wnd );
        // get the current pixel format index  
        int iPixelFormat = GetPixelFormat(hdc); 

        memset( &m_pfd, 0, sizeof(m_pfd) );
        // obtain a detailed description of that pixel format  
        DescribePixelFormat( hdc, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &m_pfd );
        ReleaseDC( wnd, hdc );

        if( !SetPixelFormat( dc, iPixelFormat, &m_pfd ) )
        {
            m_previousErrorCode = GetLastError();
            ReleaseDC( m_winID, dc );
            return;
        }
    }
#endif
    m_handle = wglCreateContext( dc );
    if( m_handle == NULL )
        m_previousErrorCode = GetLastError();
    ReleaseDC( m_winID, dc );
    dc = NULL;
    if( m_handle == NULL )
        return;

    if( contextHandleToShareWith == NULL )
        return;

    if( !wglShareLists( contextHandleToShareWith, m_handle ) )
    {
        m_previousErrorCode = GetLastError();
        wglDeleteContext( m_handle );
        m_handle = NULL;
    }
#else
    //creating OGL context shared with drawing context
    static int visualAttribList[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 4,
        GLX_GREEN_SIZE, 4,
        GLX_BLUE_SIZE, 4,
        None };
    XVisualInfo* visualInfo = glXChooseVisual( QX11Info::display(), QX11Info::appScreen(), visualAttribList );
    m_handle = glXCreateContext(
        QX11Info::display(),
        visualInfo,
        contextHandleToShareWith,
        true );
	m_previousErrorCode = errno;
#endif
}
