/**********************************************************
* 24 oct 2012
* a.kolesnikov
***********************************************************/

#include "decodedpicturetoopengluploadercontextpool.h"

#include <memory>

#include "decodedpicturetoopengluploader.h"


static const size_t uploadingThreadCountOverride = 1;

//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploadThread
//////////////////////////////////////////////////////////
DecodedPictureToOpenGLUploadThread::DecodedPictureToOpenGLUploadThread( QGLWidget* glWidget )
:
    m_glWidget( glWidget )
{
}

DecodedPictureToOpenGLUploadThread::~DecodedPictureToOpenGLUploadThread()
{
    m_queue.push( NULL );
    wait();

    delete m_glWidget;
    m_glWidget = NULL;
}

void DecodedPictureToOpenGLUploadThread::push( QRunnable* toRun )
{
    m_queue.push( toRun );
}

const QGLContext* DecodedPictureToOpenGLUploadThread::glContext() const
{
    return m_glWidget ? m_glWidget->context() : NULL;
}

void DecodedPictureToOpenGLUploadThread::run()
{
#if !(defined(GL_COPY_AGGREGATION) && defined(UPLOAD_TO_GL_IN_GUI_THREAD))
    m_glWidget->makeCurrent();
    Q_ASSERT( QGLContext::currentContext() == m_glWidget->context() );
#endif

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

        toRun->run();
    }

#if !(defined(GL_COPY_AGGREGATION) && defined(UPLOAD_TO_GL_IN_GUI_THREAD))
    m_glWidget->doneCurrent();
#endif
    NX_LOG( QString::fromAscii("DecodedPictureToOpenGLUploadThread stopped"), cl_logDEBUG1 );
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
    QMutexLocker lk( &m_mutex );

    UploaderPoolContext& pool = m_auxiliaryGLContextPool[shareWidget->context()];

    if( pool.uploaders.empty() )
    {
        if( poolSizeIncrement < 0 )
            poolSizeIncrement = m_optimalGLContextPoolSize;
        for( int i = 0; i < poolSizeIncrement; ++i )
        {
#if !(defined(GL_COPY_AGGREGATION) && defined(UPLOAD_TO_GL_IN_GUI_THREAD))
            //creating gl context (inside QGLWidget)
            std::auto_ptr<QGLWidget> newWidget( new QGLWidget( NULL, shareWidget ) );
            if( !newWidget->isSharing() )
                break;
            QSharedPointer<DecodedPictureToOpenGLUploadThread> uploadThread( new DecodedPictureToOpenGLUploadThread( newWidget.release() ) );
#else
            //no need to create additional context
            QSharedPointer<DecodedPictureToOpenGLUploadThread> uploadThread( new DecodedPictureToOpenGLUploadThread( NULL ) );
#endif
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
}

const std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& DecodedPictureToOpenGLUploaderContextPool::getPoolOfContextsSharedWith( const QGLContext* const parentContext ) const
{
    QMutexLocker lk( &m_mutex );

    std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& pool = m_auxiliaryGLContextPool[parentContext].uploaders;
    return pool;
}

DecodedPictureToOpenGLUploaderContextPool* DecodedPictureToOpenGLUploaderContextPool::instance()
{
    return qn_decodedPictureToOpenGLUploaderContextPool();
}

void DecodedPictureToOpenGLUploaderContextPool::onShareWidgetDestroyed( QObject* obj )
{
    QMutexLocker lk( &m_mutex );
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
}
