/**********************************************************
* 24 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef DECODEDPICTURETOOPENGLUPLOADERCONTEXTPOOL_H
#define DECODEDPICTURETOOPENGLUPLOADERCONTEXTPOOL_H

#include <deque>

#include <QtOpenGL/QGLWidget>
#include <utils/common/mutex.h>
#include <QtCore/QRunnable>
#include <QtCore/QThread>
#include <utils/common/wait_condition.h>

#include <utils/common/safepool.h>
#include <utils/common/threadqueue.h>


class GLContext;

class DecodedPictureToOpenGLUploader;
class UploadFrameRunnable
:
    public QRunnable
{
public:
    //DecodedPictureToOpenGLUploader* () const;
};

class DecodedPictureToOpenGLUploadThread
:
    public QThread
{
public:
    /*!
        \param glContextToUse Destroyed during object destruction
    */
    DecodedPictureToOpenGLUploadThread( GLContext* glContextToUse );
    virtual ~DecodedPictureToOpenGLUploadThread();

    void push( UploadFrameRunnable* toRun );
    const GLContext* glContext() const;

protected:
    virtual void run();

private:
    //CLThreadQueue<QRunnable*> m_taskQueue;
    GLContext* m_glContext;
    std::deque<UploadFrameRunnable*> m_taskQueue;
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_cond;
};

//!Pool of ogl contexts, used to upload decoded pictures to opengl textures
/*!
    We need additional contexts (shared with application's main context) to be able to perform uploading in non-GUI thread
*/
class DecodedPictureToOpenGLUploaderContextPool
:
    public QObject
{
    Q_OBJECT

public:
    DecodedPictureToOpenGLUploaderContextPool();
    virtual ~DecodedPictureToOpenGLUploaderContextPool();

    //!Checks, whether there are any contexts in pool shared with \a parentContext
    /*!
        If there are no contexts shared with \a parentContext, creates one (in this case \a winID is used)
        \param winID Passed to \a GLContext constructor
        \param poolSizeIncrement Number of gl contexts to create if pool is empty. If < 0, pool decides itself how much context to create
        \note It is recommended to call this method from GUI thread. In other case, behavour can be implementation-specific
    */
    bool ensureThereAreContextsSharedWith(
        QGLWidget* const glWidget,
        int poolSizeIncrement = -1 );
    /*!
        If on pool shared with \a parentContexts, an empty pool is created and returned
    */
    const std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& getPoolOfContextsSharedWith( const QGLContext* const parentContext ) const;

    static DecodedPictureToOpenGLUploaderContextPool* instance();

private:
    struct UploaderPoolContext
    {
        std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> > uploaders;
        QGLWidget* shareWidget;

        UploaderPoolContext();
    };

    mutable QnMutex m_mutex;
    //map<parent context, pool of contexts shared with parent>
    mutable std::map<const QGLContext*, UploaderPoolContext> m_auxiliaryGLContextPool;
    WId m_paintWindowId;
    size_t m_optimalGLContextPoolSize;

private slots:
    void onShareWidgetDestroyed( QObject* );
};

#endif  //DECODEDPICTURETOOPENGLUPLOADERCONTEXTPOOL_H
