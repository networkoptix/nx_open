/**********************************************************
* 24 oct 2012
* a.kolesnikov
***********************************************************/

#include "decodedpicturetoopengluploadercontextpool.h"

#include <memory>


static const size_t uploadingThreadCountOverride = 1;

//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploadThread
//////////////////////////////////////////////////////////
DecodedPictureToOpenGLUploadThread::DecodedPictureToOpenGLUploadThread( GLContext* glContext )
:
    m_glContext( glContext )
{
}

DecodedPictureToOpenGLUploadThread::~DecodedPictureToOpenGLUploadThread()
{
    m_queue.push( NULL );
    wait();

    delete m_glContext;
    m_glContext = NULL;
}

void DecodedPictureToOpenGLUploadThread::push( QRunnable* toRun )
{
    m_queue.push( toRun );
}

const GLContext* DecodedPictureToOpenGLUploadThread::glContext() const
{
    return m_glContext;
}

GLContext* DecodedPictureToOpenGLUploadThread::glContext()
{
    return m_glContext;
}

void DecodedPictureToOpenGLUploadThread::run()
{
    //GLContext::ScopedContextUsage scu( m_glContext );
    //if( !scu.isCurrent() )
    //{
    //    NX_LOG( QString::fromAscii("Failed to make gl context current %1").arg(m_glContext->getLastErrorString()), cl_logWARNING );
    //    return;
    //}

    NX_LOG( QString::fromAscii("DecodedPictureToOpenGLUploadThread started"), cl_logDEBUG1 );

    for( ;; )
    {
        QRunnable* toRun = NULL;
        bool get = m_queue.pop( toRun, 200 );
        if( !get )
            continue;

        if( !toRun )
            break;

        std::auto_ptr<QRunnable> toRunDeleter( toRun->autoDelete() ? toRun : NULL );

        GLContext::ScopedContextUsage scu( m_glContext );
        if( !scu.isCurrent() )
        {
            NX_LOG( QString::fromAscii("Failed to make gl context current %1").arg(m_glContext->getLastErrorString()), cl_logWARNING );
            return;
        }

        toRun->run();
    }

    NX_LOG( QString::fromAscii("DecodedPictureToOpenGLUploadThread stopped"), cl_logDEBUG1 );
}


//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploaderContextPool
//////////////////////////////////////////////////////////
Q_GLOBAL_STATIC( DecodedPictureToOpenGLUploaderContextPool, qn_decodedPictureToOpenGLUploaderContextPool );

DecodedPictureToOpenGLUploaderContextPool::DecodedPictureToOpenGLUploaderContextPool()
:
    m_paintWindowId( 0 ),
    m_optimalGLContextPoolSize( 0 )
{
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    memset( &sysInfo, 0, sizeof(sysInfo) );
    GetSystemInfo( &sysInfo );
    m_optimalGLContextPoolSize = std::min<size_t>( uploadingThreadCountOverride, sysInfo.dwNumberOfProcessors );
#else
    //TODO/IMPL calculate optimal pool size
    m_optimalGLContextPoolSize = uploadingThreadCountOverride;
#endif
}

DecodedPictureToOpenGLUploaderContextPool::~DecodedPictureToOpenGLUploaderContextPool()
{
    m_auxiliaryGLContextPool.clear();
}

void DecodedPictureToOpenGLUploaderContextPool::setPaintWindowHandle( WId paintWindowId )
{
    QMutexLocker lk( &m_mutex );    //while this mutex is locked no change to pool context can occur

#ifdef _WIN32
    if( paintWindowId )
        Q_ASSERT( IsWindow( paintWindowId ) );
#endif

    if( m_paintWindowId )
    {
        //deleting contexts using window m_paintWindowId
        for( std::map<GLContext::SYS_GL_CTX_HANDLE, std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> > >::iterator
            it = m_auxiliaryGLContextPool.begin();
            it != m_auxiliaryGLContextPool.end();
            ++it )
        {
            std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& contexts = it->second;
            for( std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >::iterator
                jIt = contexts.begin();
                jIt != contexts.end();
                 )
            {
                if( (*jIt)->glContext()->wnd() != m_paintWindowId )
                {
                    ++jIt;
                    continue;
                }
                //context created on window being deleted
                jIt = contexts.erase( jIt );
            }
        }
    }

    m_paintWindowId = paintWindowId;

#ifdef _WIN32
    if( m_paintWindowId )
        Q_ASSERT( IsWindow(m_paintWindowId) );
#endif
}

void DecodedPictureToOpenGLUploaderContextPool::setPaintWindow( QWidget* const paintWindow )
{
    setPaintWindowHandle( paintWindow ? paintWindow->winId() : NULL );
    //if( paintWindow )
    //    paintWindow->installEventFilter( this );
}

WId DecodedPictureToOpenGLUploaderContextPool::paintWindowHandle() const
{
    return m_paintWindowId;
}

bool DecodedPictureToOpenGLUploaderContextPool::ensureThereAreContextsSharedWith(
    GLContext::SYS_GL_CTX_HANDLE parentContextID,
    WId winID,
    int poolSizeIncrement )
{
    QMutexLocker lk( &m_mutex );

    std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& pool = m_auxiliaryGLContextPool[parentContextID];

    if( pool.empty() )
    {
        if( poolSizeIncrement < 0 )
            poolSizeIncrement = m_optimalGLContextPoolSize;
        for( int i = 0; i < poolSizeIncrement; ++i )
        {
            //creating context
            std::auto_ptr<GLContext> glCtx( new GLContext( winID ? winID : m_paintWindowId, parentContextID ) );
            if( !glCtx->isValid() )
                break;
            QSharedPointer<DecodedPictureToOpenGLUploadThread> uploadThread( new DecodedPictureToOpenGLUploadThread( glCtx.release() ) );
            uploadThread->start();
            if( !uploadThread->isRunning() )
                break;
            pool.push_back( uploadThread );
        }
    }

    return !pool.empty();
}

const std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& DecodedPictureToOpenGLUploaderContextPool::getPoolOfContextsSharedWith( GLContext::SYS_GL_CTX_HANDLE parentContext ) const
{
    QMutexLocker lk( &m_mutex );

#ifdef _WIN32
    Q_ASSERT( IsWindow(m_paintWindowId) );
#endif

    std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& pool = m_auxiliaryGLContextPool[parentContext];
    return pool;
}

DecodedPictureToOpenGLUploaderContextPool* DecodedPictureToOpenGLUploaderContextPool::instance()
{
    return qn_decodedPictureToOpenGLUploaderContextPool();
}
