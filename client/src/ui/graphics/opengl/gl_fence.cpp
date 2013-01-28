/**********************************************************
* 23 jan 2013
* a.kolesnikov
***********************************************************/

#include "gl_fence.h"

#include <ui/graphics/opengl/gl_functions.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>


GLFence::GLFence( QnGlFunctions* const glFunctions )
:
    m_fenceSyncName( 0 ),
    //m_arbSyncPresent( glFunctions->features() & QnGlFunctions::ARB_Sync ),
    m_arbSyncPresent( false ),  //TODO/IMPL need to enable this, but before testing on different video cards is needed
    m_glFunctions( glFunctions )
{
}

GLFence::~GLFence()
{
    if( m_fenceSyncName )
        m_glFunctions->glDeleteSync( (GLsync)m_fenceSyncName );
}

bool GLFence::placeFence()
{
    if( m_arbSyncPresent )
    {
        if( m_fenceSyncName )
            m_glFunctions->glDeleteSync( (GLsync)m_fenceSyncName );
        m_fenceSyncName = m_glFunctions->glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
        return m_fenceSyncName != 0;
    }
    else
    {
        glFlush();
        glFinish();
        return true;
    }
}

void GLFence::sync()
{
    if( !m_arbSyncPresent || !m_fenceSyncName )
        return;

    for( ;; )
    {
        switch( m_glFunctions->glClientWaitSync( (GLsync)m_fenceSyncName, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000 ) )
        {
            case GL_ALREADY_SIGNALED:
            case GL_CONDITION_SATISFIED:
            case GL_WAIT_FAILED:
                m_glFunctions->glDeleteSync( (GLsync)m_fenceSyncName );
                m_fenceSyncName = 0;
                return;

            case GL_TIMEOUT_EXPIRED:
                continue;

            default:
                Q_ASSERT( false );
                return;
        }
    }
}
