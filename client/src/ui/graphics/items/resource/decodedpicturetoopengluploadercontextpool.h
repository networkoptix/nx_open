/**********************************************************
* 24 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef DECODEDPICTURETOOPENGLUPLOADERCONTEXTPOOL_H
#define DECODEDPICTURETOOPENGLUPLOADERCONTEXTPOOL_H

#include <QRunnable>
#include <QThread>
#include <QWidget>

#include <utils/common/safepool.h>
#include <utils/common/threadqueue.h>
#include <utils/gl/glcontext.h>


class DecodedPictureToOpenGLUploadThread
:
    public QThread
{
public:
    /*!
        \param glContext Destroyed during object destruction
    */
    DecodedPictureToOpenGLUploadThread( GLContext* glContext );
    virtual ~DecodedPictureToOpenGLUploadThread();

    void push( QRunnable* toRun );
    const GLContext* glContext() const;
    GLContext* glContext();

protected:
    virtual void run();

private:
    CLThreadQueue<QRunnable*> m_queue;
    GLContext* m_glContext;
};

//!Pool of ogl contexts, used to upload decoded pictures to opengl textures
/*!
    We need additional contexts (shared with application's main context) to be able to perform uploading in non-GUI thread
*/
class DecodedPictureToOpenGLUploaderContextPool
{
public:
    DecodedPictureToOpenGLUploaderContextPool();
    virtual ~DecodedPictureToOpenGLUploaderContextPool();

    //!Set id of window to use for gl context creation
    void setPaintWindowHandle( WId paintWindowId );
    WId paintWindowHandle() const;
    void setPaintWindow( QWidget* const paintWindow );
    //!Checks, whether there are any contexts in pool shared with \a parentContext
    /*!
        If there are no contexts shared with \a parentContext, creates one (in this case \a winID is used)
        \param winID Passed to \a GLContext constructor
        \param poolSizeIncrement Number of gl contexts to create if pool is empty. If < 0, pool decides itself how much context to create
        \note It is recommended to call this method from GUI thread. In other case, behavour can be implementation-specific
    */
    bool ensureThereAreContextsSharedWith(
        GLContext::SYS_GL_CTX_HANDLE parentContext,
        WId winID = 0,
        int poolSizeIncrement = -1 );
    /*!
        If on pool shared with \a parentContexts, an empty pool is created and returned
    */
    const std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& getPoolOfContextsSharedWith( GLContext::SYS_GL_CTX_HANDLE parentContext ) const;

    static DecodedPictureToOpenGLUploaderContextPool* instance();

private:
    mutable QMutex m_mutex;
    //map<parent context, pool of contexts shared with parent>
    mutable std::map<GLContext::SYS_GL_CTX_HANDLE, std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> > > m_auxiliaryGLContextPool;
    WId m_paintWindowId;
    size_t m_optimalGLContextPoolSize;
};

#endif  //DECODEDPICTURETOOPENGLUPLOADERCONTEXTPOOL_H
