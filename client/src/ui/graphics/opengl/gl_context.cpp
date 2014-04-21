/**********************************************************
* 14 oct 2012
* a.kolesnikov
***********************************************************/

#include "gl_context.h"

#ifdef NX_GLCONTEXT_PRESENT

#pragma comment(lib,"opengl32.lib")

#include <QtOpenGL/QGLContext>

#include <utils/common/log.h>

#ifdef Q_OS_LINUX
#include <QtX11Extras/QX11Info>
#include <GL/glx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#endif


#ifdef Q_OS_WIN
    #include <utils/qt5port_win.h>
#endif


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
#ifdef Q_OS_WIN
    Q_ASSERT( IsWindow(widToHwnd(m_winID)) );

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

#ifdef Q_OS_WIN
    if( paintDevToUse != NULL )
    {
        m_dc = paintDevToUse;
    }
    else
    {
        Q_ASSERT( IsWindow( widToHwnd(m_winID) ) );
        m_dc = GetDC( widToHwnd(m_winID) );
    }
    BOOL res = wglMakeCurrent( m_dc, m_handle );
    m_previousErrorCode = GetLastError();
    if( paintDevToUse != NULL )
        m_dc = NULL;    //no need to release device context at doneCurrent
    return res;
#else
    Q_UNUSED(paintDevToUse)
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
#ifdef Q_OS_WIN
    wglMakeCurrent( NULL, NULL );
    if( m_dc != NULL )
        ReleaseDC( widToHwnd(m_winID), m_dc );
    m_dc = NULL;
#else
    glXMakeCurrent(
        QX11Info::display(),
        None,
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
#ifdef Q_OS_WIN
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

#ifdef Q_OS_WIN
    SYS_GL_CTX_HANDLE sysContextHandle = wglGetCurrentContext();
#else
    SYS_GL_CTX_HANDLE sysContextHandle = glXGetCurrentContext();
#endif

    if( paintDeviceHandle )
    {
#ifdef Q_OS_WIN
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
    m_widget.reset( new QWidget() );

    m_winID = m_widget->winId();

#ifdef Q_OS_WIN
    Q_ASSERT( IsWindow( widToHwnd(m_winID) ) );

    HDC dc = GetDC( widToHwnd(m_winID) );
    //reading pixel format of src window
    {
        HDC hdc = GetDC( widToHwnd(wnd) );
        // get the current pixel format index  
        int iPixelFormat = GetPixelFormat(hdc); 

        memset( &m_pfd, 0, sizeof(m_pfd) );
        // obtain a detailed description of that pixel format  
        DescribePixelFormat( hdc, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &m_pfd );
        ReleaseDC( widToHwnd(wnd), hdc );

        if( !SetPixelFormat( dc, iPixelFormat, &m_pfd ) )
        {
            m_previousErrorCode = GetLastError();
            ReleaseDC( widToHwnd(m_winID), dc );
            return;
        }
    }
    //TODO/IMPL no need to request function address every time
    /*PFNWGLCREATECONTEXTATTRIBSARBPROC my_wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress( "wglCreateContextAttribsARB" );
    if( my_wglCreateContextAttribsARB )
    {
        //creating OpenGL 3.0 context
        static const int contextAttribs[] =
        {
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
            0
        }; 
        m_handle = my_wglCreateContextAttribsARB( dc, contextHandleToShareWith, contextAttribs );
    }
    else*/
    {
        //creating Opengl 1.0 context
        m_handle = wglCreateContext( dc );
        if( m_handle != NULL &&
            contextHandleToShareWith != NULL &&
            !wglShareLists( contextHandleToShareWith, m_handle ) )
        {
            m_previousErrorCode = GetLastError();
            wglDeleteContext( m_handle );
            m_handle = NULL;
        }
    }

    ReleaseDC( widToHwnd(m_winID), dc );
    dc = NULL;

    if( m_handle == NULL )
        m_previousErrorCode = GetLastError();

    //if( contextHandleToShareWith == NULL )
    //    return;

    //if( !wglShareLists( contextHandleToShareWith, m_handle ) )
    //{
    //    m_previousErrorCode = GetLastError();
    //    wglDeleteContext( m_handle );
    //    m_handle = NULL;
    //}
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

#endif  //NX_GLCONTEXT_PRESENT
