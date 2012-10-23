/**********************************************************
* 08 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef DECODEDPICTURETOOPENGLUPLOADER_H
#define DECODEDPICTURETOOPENGLUPLOADER_H

#include <deque>
#include <memory>
#include <set>
#include <vector>

#include <QGLContext>
#include <QMutex>
#include <QSharedPointer>
#include <QWaitCondition>

#include <core/datapacket/media_data_packet.h> /* For QnMetaDataV1Ptr. */
#include <utils/common/safepool.h>
#include <utils/gl/glcontext.h>
#include <utils/media/frame_info.h>


class GLContext;
class CLVideoDecoderOutput;
class DecodedPictureToOpenGLUploaderPrivate;
class QnGlRendererTexture;

class DecodedPictureToOpenGLUploaderContextPool
:
    public QObject
{
    Q_OBJECT

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
        WId winID = NULL,
        int poolSizeIncrement = -1 );
    /*!
        If on pool shared with \a parentContexts, an empty pool is created and returned
    */
    const SafePool<GLContext*, QSharedPointer<GLContext> >& getPoolOfContextsSharedWith( GLContext::SYS_GL_CTX_HANDLE parentContext ) const;

    //!Implementation of QObject::eventFilter. Deleted all GLContexts, associated with window being closed
    //virtual bool eventFilter( QObject* watched, QEvent* event );

    static DecodedPictureToOpenGLUploaderContextPool* instance();

public slots:
    //!Equal to setPaintWindowHandle( NULL )
    void resetPaintWindowHandle();

private:
    mutable QMutex m_mutex;
    //map<parent context, pool of contexts shared with parent>
    mutable std::map<GLContext::SYS_GL_CTX_HANDLE, SafePool<GLContext*, QSharedPointer<GLContext> >* > m_auxiliaryGLContextPool;
    WId m_paintWindowId;
    size_t m_optimalGLContextPoolSize;
};

class AsyncUploader;

//!Used by decoding thread to load decoded picture from system memory to opengl texture(s)
/*!
    - loads decoded picture from system memory to opengl texture(s)
    - tracks usage of texture(s) by GUI thread
    - opengl context must be created before instanciation of this class
    - supports multiple opengl textures (one is being loaded to by decoder thread, another is used for rendering by GUI thread)

    \note All methods are thread-safe
    \note GUI thread does not get blocked by decoder thread if \a asyncDepth > 0
    \note this class provides decoded picture to renderer in order they were passed to it
    \note Single instance of \a DecodedPictureToOpenGLUploader can manage decoded pictures of same type. 
        Calling \a uploadDecodedPicture with pictures of different types may lead to undefined behavour
*/
class DecodedPictureToOpenGLUploader
{
public:
    //!Represents picture, ready to be displayed. It is opengl texture(s)
    /*!
        \note This class is not thread-safe
    */
    class UploadedPicture
    {
        friend class DecodedPictureToOpenGLUploader;
        friend class AsyncUploader;

    public:
        const SafePool<GLContext*, QSharedPointer<GLContext> >& glContextPool() const;
        GLContext* glContext() const;
        PixelFormat colorFormat() const;
        void setColorFormat( PixelFormat newFormat );
        int width() const;
        int height() const;
        const QVector2D& texCoords() const;
        /*!
            Number of textures depends on color format:\n
                - 3 for YV12, one for each plane
                - 2 for NV12, one for each plane (Y- and UV-)
                - 1 for rgb
        */
        const std::vector<GLuint>& glTextures() const;
        //!Serial number of decoded picture (always different from 0)
        unsigned int sequence() const;
        quint64 pts() const;
        QnMetaDataV1Ptr metadata() const;
        //!Returns opengl texture holding plane \a index data. Index of a plane depends on color format (Y, U, V for YV12; Y, UV for NV12 and RGB for rgb format)
        QnGlRendererTexture* texture( int index ) const;

    private:
        static const int TEXTURE_COUNT = 3;

        const SafePool<GLContext*, QSharedPointer<GLContext> >& m_glContextPool;
        GLContext* const m_glContext;
        PixelFormat m_colorFormat;
        int m_width;
        int m_height;
        mutable std::vector<GLuint> m_picTextures;
        QScopedPointer<QnGlRendererTexture> m_textures[TEXTURE_COUNT];
        unsigned int m_sequence;
        quint64 m_pts;
        QnMetaDataV1Ptr m_metadata;

        UploadedPicture(
            DecodedPictureToOpenGLUploader* const uploader,
            const SafePool<GLContext*, QSharedPointer<GLContext> >& _glContextPool,
            GLContext* const _glContext );
        UploadedPicture( const UploadedPicture& );
        UploadedPicture& operator=( const UploadedPicture& );
        ~UploadedPicture();
    };

    //!Calls \a getUploadedPicture at initialiation and \a pictureDrawingFinished before destruction
    class ScopedPictureLock
    {
    public:
        ScopedPictureLock( const DecodedPictureToOpenGLUploader& uploader );
        ~ScopedPictureLock();

        UploadedPicture* operator->();
        const UploadedPicture* operator->() const;
        UploadedPicture* get();
        const UploadedPicture* get() const;

    private:
        const DecodedPictureToOpenGLUploader& m_uploader;
        UploadedPicture* m_picture;
    };

    /*!
        \param mainContext Application's main gl context. Used to find address of opengl extension functions. Required, because \a QnGlFunctions is tied to \a QGLContext.
            In future, this parameter should be removed (this will required \a QnGlFunctions refactoring)
        \param mainContextHandle System-dependent handle of \a mainContext. This argument is introduced to allow \a DecodedPictureToOpenGLUploader
            instanciation from non-GUI thread (since we can get gl context handle only in GUI thread)
        \param asyncDepth Number of additional uploaded picture textures to allocate so that decrease wait for rendering by decoder thread
        If \a asyncDepth is 0, only one uploded picture data is allocated so \a uploadDecodedPicture will block while GUI thread renders picture

        \note If \a asyncDepth is 0 then it is possible that getUploadedPicture() will return NULL (if the only gl buffer is currently used for uploading)
    */
    DecodedPictureToOpenGLUploader(
        const QGLContext* const mainContext,
        GLContext::SYS_GL_CTX_HANDLE mainContextHandle,
        unsigned int asyncDepth = 1 );
    ~DecodedPictureToOpenGLUploader();

    //!Uploads \a decodedPicture to opengl texture(s). Used by decoder thread
    /*!
        Blocks till upload is done
        \note Method does not save reference to \a decodedPicture, but it can save reference to \a decodedPicture->picData. 
            As soon as uploader is done with \a decodedPicture->picData it releases reference to it
    */
    void uploadDecodedPicture( const QSharedPointer<CLVideoDecoderOutput>& decodedPicture );
    //!Returns uploaded picture. Used by GUI thread
    /*!
        This method calls UploadedPicture::AddRef, so renderer must call only releaseRef when it's done rendering.
        \return Uploaded picture data, NULL if no picture. Returned object memory is managed by \a DecodedPictureToOpenGLUploader, and MUST NOT be deleted
    */
    UploadedPicture* getUploadedPicture() const;
    //!Marks \a picture as empty
    /*!
        Renderer MUST call this method to signal that \a picture can be used for uploading next decoded frame
    */
    void pictureDrawingFinished( UploadedPicture* const picture ) const;
    void setForceSoftYUV( bool value );
    bool isForcedSoftYUV() const;
    qreal opacity() const;
    void setOpacity( qreal opacity );
    /*!
        In this method occures cleanup of all resources. Object is unusable futher...
    */
    void beforeDestroy();
    void setYV12ToRgbShaderUsed( bool yv12SharedUsed );
    void setNV12ToRgbShaderUsed( bool nv12SharedUsed );

    //!This method is called by asynchronous uploader when upload is done
    void pictureDataUploadSucceeded( AsyncUploader* const uploader, UploadedPicture* const picture );
    //!This method is called by asynchronous uploader when upload has failed
    void pictureDataUploadFailed( AsyncUploader* const uploader, UploadedPicture* const picture );
    //!Uploader calles this method, if picture uploading has been cancelled from outside
    void pictureDataUploadCancelled( AsyncUploader* const uploader );

    //!Loads picture with dat stored in \a planes to opengl \a dest
    /*!
        \param planes Array size must be >= 3. Although, only necessary planes are used (3 for YV12, 2 for NV12)
        \param lineSizes Array size must be >= 3
        \param isVideoMemory Should be set to true when data in \a planes is in USWC memory to optimize data reading. false otherwise. Currently unused

        \note Method is re-enterable
        \return false, if failed to make \a glContext current. Otherwise, true
    */
    bool uploadDataToGl(
        GLContext* const glContext,
        UploadedPicture* const dest,
        PixelFormat format,
        unsigned int width,
        unsigned int height,
        uint8_t* planes[],
        int lineSizes[],
        bool isVideoMemory );

private:
    friend class QnGlRendererTexture;

    QSharedPointer<DecodedPictureToOpenGLUploaderPrivate> d;
    int m_format;
    uchar* m_yuv2rgbBuffer;
    int m_yuv2rgbBufferLen;
    bool m_forceSoftYUV;
    qreal m_painterOpacity;
    mutable QMutex m_mutex;
    mutable QMutex m_uploadMutex;
    mutable QWaitCondition m_cond;
    unsigned int m_previousPicSequence;
    std::deque<UploadedPicture*> m_emptyBuffers;
    mutable std::deque<UploadedPicture*> m_renderedPictures;
    mutable std::deque<UploadedPicture*> m_picturesWaitingRendering;
    mutable std::deque<UploadedPicture*> m_picturesBeingRendered;
    bool m_terminated;
    bool m_yv12SharedUsed;
    bool m_nv12SharedUsed;
    mutable std::deque<AsyncUploader*> m_unusedUploaders;
    std::deque<AsyncUploader*> m_usedUploaders;

    bool updateTextures( UploadedPicture* const emptyPictureBuf, const QSharedPointer<CLVideoDecoderOutput>& curImg );
    bool usingShaderYuvToRgb() const;
    bool usingShaderNV12ToRgb() const;
    void releaseDecodedPicturePool( std::deque<UploadedPicture*>* const pool );
    //!Method is not thread-safe
    unsigned int nextPicSequenceValue();
};

class AuxiliaryGLContextDeleter
:
    public QObject
{
    Q_OBJECT

public:
    virtual bool event( QEvent* e );
};

#endif  //DECODEDPICTURETOOPENGLUPLOADER_H
