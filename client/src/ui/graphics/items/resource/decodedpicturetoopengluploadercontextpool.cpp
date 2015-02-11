/**********************************************************
* 24 oct 2012
* a.kolesnikov
***********************************************************/

#include "decodedpicturetoopengluploadercontextpool.h"

#include <memory>

#include <utils/common/log.h>

#include <ui/graphics/opengl/gl_context.h>

#include "decodedpicturetoopengluploader.h"


static const size_t uploadingThreadCountOverride = 1;

//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploadThread
//////////////////////////////////////////////////////////
DecodedPictureToOpenGLUploadThread::DecodedPictureToOpenGLUploadThread( GLContext* glContextToUse )
:
    m_glContext( glContextToUse )
{
    setObjectName( QString::fromLatin1("DecodedPictureToOpenGLUploadThread") );
}

DecodedPictureToOpenGLUploadThread::~DecodedPictureToOpenGLUploadThread()
{
#ifdef NX_GLCONTEXT_PRESENT
    push( NULL );
    wait();

    delete m_glContext;
    m_glContext = NULL;
#endif
}

void DecodedPictureToOpenGLUploadThread::push( UploadFrameRunnable* toRun )
{
#ifdef NX_GLCONTEXT_PRESENT
    //m_taskQueue.push( toRun );
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    m_taskQueue.push_back( toRun );
    m_cond.wakeAll();
#endif
}

const GLContext* DecodedPictureToOpenGLUploadThread::glContext() const
{
    return m_glContext;
}

void DecodedPictureToOpenGLUploadThread::run()
{
#ifdef NX_GLCONTEXT_PRESENT
#if !(defined(GL_COPY_AGGREGATION) && defined(UPLOAD_TO_GL_IN_GUI_THREAD))
    GLContext::ScopedContextUsage scu( m_glContext );
#endif

    NX_LOG( QString::fromLatin1("DecodedPictureToOpenGLUploadThread started"), cl_logDEBUG1 );

    for( ;; )
    {
        UploadFrameRunnable* toRun = NULL;
        //bool get = m_taskQueue.pop( toRun, 200 );
        //if( !get )
        //    continue;
        {
            SCOPED_MUTEX_LOCK( lk,  &m_mutex );
            while( m_taskQueue.empty() )
                m_cond.wait( lk.mutex() );
            toRun = m_taskQueue.front();
            m_taskQueue.pop_front();
        }

        if( !toRun )
            break;

        std::auto_ptr<UploadFrameRunnable> toRunDeleter( toRun->autoDelete() ? toRun : NULL );
        toRun->run();
    }

    NX_LOG( QString::fromLatin1("DecodedPictureToOpenGLUploadThread stopped"), cl_logDEBUG1 );
#endif
}


//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploaderContextPool
//////////////////////////////////////////////////////////
Q_GLOBAL_STATIC( DecodedPictureToOpenGLUploaderContextPool, qn_decodedPictureToOpenGLUploaderContextPool );

DecodedPictureToOpenGLUploaderContextPool::UploaderPoolContext::UploaderPoolContext()
:
    shareWidget( NULL )
{
}

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

bool DecodedPictureToOpenGLUploaderContextPool::ensureThereAreContextsSharedWith(
    QGLWidget* const shareWidget,
    int poolSizeIncrement )
{
#ifdef NX_GLCONTEXT_PRESENT
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    UploaderPoolContext& pool = m_auxiliaryGLContextPool[shareWidget->context()];

    if( pool.uploaders.empty() )
    {
        if( poolSizeIncrement < 0 )
            poolSizeIncrement = m_optimalGLContextPoolSize;
        for( int i = 0; i < poolSizeIncrement; ++i )
        {
//#if !(defined(GL_COPY_AGGREGATION) && defined(UPLOAD_TO_GL_IN_GUI_THREAD))
            //creating gl context (inside QGLWidget)
            std::auto_ptr<GLContext> newContext( new GLContext( shareWidget ) );
            QSharedPointer<DecodedPictureToOpenGLUploadThread> uploadThread( new DecodedPictureToOpenGLUploadThread( newContext.release() ) );
//#else
//            //no need to create additional context
//            QSharedPointer<DecodedPictureToOpenGLUploadThread> uploadThread( new DecodedPictureToOpenGLUploadThread( NULL ) );
//#endif
            uploadThread->start();
            if( !uploadThread->isRunning() )
                break;
            pool.uploaders.push_back( uploadThread );
        }

        //TODO/IMPL monitor shareWidget life-time and destroy created widgets
        connect( shareWidget, SIGNAL(destroyed(QObject*)), this, SLOT(onShareWidgetDestroyed(QObject*)) );
        pool.shareWidget = shareWidget;
    }

    return !pool.uploaders.empty();
#else
    return false;
#endif
}

const std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& DecodedPictureToOpenGLUploaderContextPool::getPoolOfContextsSharedWith( const QGLContext* const parentContext ) const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& pool = m_auxiliaryGLContextPool[parentContext].uploaders;
    return pool;
}

DecodedPictureToOpenGLUploaderContextPool* DecodedPictureToOpenGLUploaderContextPool::instance()
{
    return qn_decodedPictureToOpenGLUploaderContextPool();
}

void DecodedPictureToOpenGLUploaderContextPool::onShareWidgetDestroyed( QObject* obj )
{
#ifdef NX_GLCONTEXT_PRESENT
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    for( std::map<const QGLContext*, UploaderPoolContext>::const_iterator
        it = m_auxiliaryGLContextPool.begin();
        it != m_auxiliaryGLContextPool.end();
        ++it )
    {
        if( it->second.shareWidget == obj )
        {
            m_auxiliaryGLContextPool.erase( it );
            return;
        }
    }
#endif
}
