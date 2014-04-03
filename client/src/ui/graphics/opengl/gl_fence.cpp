/**********************************************************
* 23 jan 2013
* a.kolesnikov
***********************************************************/

#include "gl_fence.h"

#include <ui/graphics/opengl/gl_functions.h>

//#define GL_GLEXT_PROTOTYPES
//#ifdef Q_OS_MACX
//#include <glext.h>
//#else
//#include <GL/glext.h>
//#endif
//#define GL_GLEXT_PROTOTYPES 1
#include <QtGui/qopengl.h>
#include <QtGui/QOpenGLFunctions_3_2_Core>


GLFence::GLFence() :
    m_fenceSyncName( 0 ),
//    m_versionFunctions(NULL),
    m_versionFunctionsInitialized(false)
{
}

GLFence::~GLFence()
{
    if (!m_fenceSyncName)
        return;

 //   versionFunctions()->glDeleteSync((GLsync)m_fenceSyncName);
}

bool GLFence::placeFence()
{
   /* if( arbSyncPresent() )
    {
        if( m_fenceSyncName )
            versionFunctions()->glDeleteSync( (GLsync)m_fenceSyncName );
        m_fenceSyncName = versionFunctions()->glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
        return m_fenceSyncName != 0;
    }
    else
    {
        glFlush();
        glFinish();
        return true;
    }*/
    return false;
}

void GLFence::sync()
{
/*    if( !arbSyncPresent() || !m_fenceSyncName )
        return;

    for( ;; )
    {
        switch( versionFunctions()->glClientWaitSync( (GLsync)m_fenceSyncName, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000 ) )
        {
            case GL_ALREADY_SIGNALED:
            case GL_CONDITION_SATISFIED:
            case GL_WAIT_FAILED:
                versionFunctions()->glDeleteSync( (GLsync)m_fenceSyncName );
                m_fenceSyncName = 0;
                return;

            case GL_TIMEOUT_EXPIRED:
                continue;

            default:
                Q_ASSERT( false );
                return;
        }
    }*/
}

bool GLFence::trySync()
{
 /*   if( !arbSyncPresent() || !m_fenceSyncName )
        return false;

    for( ;; )
    {
        switch( versionFunctions()->glClientWaitSync( (GLsync)m_fenceSyncName, GL_SYNC_FLUSH_COMMANDS_BIT, 0 ) )
        {
            case GL_ALREADY_SIGNALED:
            case GL_CONDITION_SATISFIED:
                versionFunctions()->glDeleteSync( (GLsync)m_fenceSyncName );
                m_fenceSyncName = 0;
                return true;

            case GL_TIMEOUT_EXPIRED:
                return false;

            case GL_WAIT_FAILED:
                return false;

            default:
                Q_ASSERT( false );
                return false;
        }
    }*/
    return false;
}
/*
QOpenGLFunctions_3_2_Core* GLFence::versionFunctions() {
    if (m_versionFunctionsInitialized)
        return m_versionFunctions;

    m_versionFunctionsInitialized = true;
    m_versionFunctions = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_3_2_Core>();
    if (!m_versionFunctions) {
        qWarning() << "Could not obtain required OpenGL context version";
        return NULL;
    }
    if (!m_versionFunctions->initializeOpenGLFunctions())
        m_versionFunctions = NULL;
    return m_versionFunctions;
}*/

bool GLFence::arbSyncPresent() {
//    return versionFunctions() != NULL;
    return false;
}
