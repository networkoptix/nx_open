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

#include <QtOpenGL/QGLContext>
#include <QtCore/QMutex>
#include <QSharedPointer>
#include <QtCore/QWaitCondition>

#include <core/datapacket/media_data_packet.h> /* For QnMetaDataV1Ptr. */
#include <utils/common/safepool.h>
#include <utils/common/stoppable.h>
#include <ui/graphics/opengl/gl_fence.h>
#include <utils/media/frame_info.h>

//#define GL_COPY_AGGREGATION
#ifdef GL_COPY_AGGREGATION
#include "aggregationsurface.h"
#define UPLOAD_TO_GL_IN_GUI_THREAD
#endif
#include "utils/color_space/image_correction.h"


class AsyncPicDataUploader;
class CLVideoDecoderOutput;
class DecodedPictureToOpenGLUploaderPrivate;
class DecodedPictureToOpenGLUploadThread;
class QnGlRendererTexture;
class QnGlRendererTexturePack;
class AVPacketUploader;

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
:
    public QnStoppable
{
public:
    //!Represents picture, ready to be displayed. It is opengl texture(s)
    /*!
        \note This class is not thread-safe
    */
    class UploadedPicture
    {
        friend class DecodedPictureToOpenGLUploader;
        friend class AsyncPicDataUploader;

    public:
        PixelFormat colorFormat() const;
        void setColorFormat( PixelFormat newFormat );
        int width() const;
        int height() const;
        //const QVector2D& texCoords() const;
        QRectF textureRect() const;
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
        QnGlRendererTexturePack* texturePack();
        //!Returns opengl texture holding plane \a index data. Index of a plane depends on color format (Y, U, V for YV12; Y, UV for NV12 and RGB for rgb format)
        QnGlRendererTexture* texture( int index ) const;
        GLuint pboID( int index ) const;
        int flags() const;
#ifdef GL_COPY_AGGREGATION
        void setAggregationSurfaceRect( const QSharedPointer<AggregationSurfaceRect>& surfaceRect );
        const QSharedPointer<AggregationSurfaceRect>& aggregationSurfaceRect() const;
#endif
        void processImage( quint8* yPlane, int width, int height, int stride, const ImageCorrectionParams& data);

        const ImageCorrectionResult& imageCorrectionResult() const;

    private:
        struct PBOData
        {
            GLuint id;
            size_t sizeBytes;

            PBOData();
        };

        PixelFormat m_colorFormat;
        int m_width;
        int m_height;
        unsigned int m_sequence;
        quint64 m_pts;
        QnMetaDataV1Ptr m_metadata;
        std::vector<PBOData> m_pbo;
        bool m_skippingForbidden;
        int m_flags;
        GLFence m_glFence;
        ImageCorrectionResult m_imgCorrection;
        QRectF m_displayedRect;
        QnGlRendererTexturePack* m_texturePack;

        UploadedPicture( DecodedPictureToOpenGLUploader* const uploader );
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
        unsigned int asyncDepth = 1 );
    virtual ~DecodedPictureToOpenGLUploader();

    //!Implementation of QnStoppable::pleaseStop()
    /*!
        Call this to inform object that no more frames will be displayed
    */
    virtual void pleaseStop() override;

    //!Uploads \a decodedPicture to opengl texture(s). Used by decoder thread
    /*!
        Blocks till upload is done
        \note Method does not save reference to \a decodedPicture, but it can save reference to \a decodedPicture->picData. 
            As soon as uploader is done with \a decodedPicture->picData it releases reference to it
    */
    void uploadDecodedPicture(
        const QSharedPointer<CLVideoDecoderOutput>& decodedPicture,
        const QRectF displayedRect = QRectF(0.0, 0.0, 1.0, 1.0) );
    bool isUsingFrame( const QSharedPointer<CLVideoDecoderOutput>& image ) const;
    //!Returns latest uploaded picture. Used by GUI thread
    /*!
        \return Uploaded picture data, NULL if no picture. Returned object memory is managed by \a DecodedPictureToOpenGLUploader, and MUST NOT be deleted
    */
    UploadedPicture* getUploadedPicture() const;
    //!Returns timestamp of next frame to display or AV_NOPTS_VALUE if upload queue is empty
    quint64 nextFrameToDisplayTimestamp() const;
    //!Blocks until all submitted frames have been rendered
    void waitForAllFramesDisplayed();
    //!Marks all posted frames as non-ignorable
    void ensureAllFramesWillBeDisplayed();
    void ensureQueueLessThen(int maxSize);
    //!Clears display queue
    /*!
        This method will not block if called from GUI thread. Otherwise, it can block till GUI thread finishes rendering of a picture
    */
    void discardAllFramesPostedToDisplay();
    //!Blocks till frame currently being rendered is done
    void waitForCurrentFrameDisplayed();
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
    void pictureDataUploadSucceeded( AsyncPicDataUploader* const uploader, UploadedPicture* const picture );
    //!This method is called by asynchronous uploader when upload has failed
    void pictureDataUploadFailed( AsyncPicDataUploader* const uploader, UploadedPicture* const picture );
    //!Uploader calles this method, if picture uploading has been cancelled from outside
    void pictureDataUploadCancelled( AsyncPicDataUploader* const uploader );

    void setImageCorrection(const ImageCorrectionParams& value);
    ImageCorrectionParams getImageCorrection() const;

    //!Loads picture with dat stored in \a planes to opengl \a dest
    /*!
        \param glContext This context MUST be current in the current thread
        \param planes Array size must be >= 3. Although, only necessary planes are used (3 for YV12, 2 for NV12)
        \param lineSizes Array size must be >= 3
        \param isVideoMemory Should be set to true when data in \a planes is in USWC memory to optimize data reading. false otherwise. Currently unused

        \note Method is re-enterable
        \return false, if failed to make \a glContext current. Otherwise, true
    */
    bool uploadDataToGl(
        UploadedPicture* const dest,
        PixelFormat format,
        unsigned int width,
        unsigned int height,
        uint8_t* planes[],
        int lineSizes[],
        bool isVideoMemory );
#ifdef GL_COPY_AGGREGATION
    bool uploadDataToGlWithAggregation(
        DecodedPictureToOpenGLUploader::UploadedPicture* const emptyPictureBuf,
        const PixelFormat format,
        const unsigned int width,
        const unsigned int height,
        uint8_t* planes[],
        int lineSizes[],
        bool /*isVideoMemory*/ );
#endif
private:
    friend class QnGlRendererTexture;
    friend class DecodedPicturesDeleter;

    QSharedPointer<DecodedPictureToOpenGLUploaderPrivate> d;
    int m_format;
    uchar* m_yuv2rgbBuffer;
    int m_yuv2rgbBufferLen;
    qreal m_painterOpacity;
    mutable QMutex m_mutex;
    mutable QMutex m_uploadMutex;
    mutable QWaitCondition m_cond;
    unsigned int m_previousPicSequence;
    mutable std::deque<UploadedPicture*> m_emptyBuffers;
    mutable std::deque<UploadedPicture*> m_renderedPictures;
    mutable std::deque<UploadedPicture*> m_picturesWaitingRendering;
    mutable std::deque<UploadedPicture*> m_picturesBeingRendered;
    mutable std::deque<AVPacketUploader*> m_framesWaitingUploadInGUIThread;
    bool m_terminated;
    mutable std::deque<AsyncPicDataUploader*> m_unusedAsyncUploaders;
    std::deque<AsyncPicDataUploader*> m_usedAsyncUploaders;
    QSharedPointer<DecodedPictureToOpenGLUploadThread> m_uploadThread;
    quint8* m_rgbaBuf;
    int m_fileNumber;
    bool m_hardwareDecoderUsed;
    bool m_asyncUploadUsed;
    QGLContext* m_initializedCtx;
    
    ImageCorrectionParams m_imageCorrection;

    bool usingShaderYuvToRgb() const;
    bool usingShaderNV12ToRgb() const;
    void releaseDecodedPicturePool( std::deque<UploadedPicture*>* const pool );
    //!Method is not thread-safe
    unsigned int nextPicSequenceValue();
    void ensurePBOInitialized(
        DecodedPictureToOpenGLUploader::UploadedPicture* const picBuf,
        unsigned int pboIndex,
        size_t sizeInBytes );
    void releasePictureBuffers();
    void releasePictureBuffersNonSafe();
    void savePicToFile( AVFrame* const pic, int pts );
    //!m_mutex MUST be locked before this call
    void cancelUploadingInGUIThread();

    DecodedPictureToOpenGLUploader( const DecodedPictureToOpenGLUploader& );
    DecodedPictureToOpenGLUploader& operator=( const DecodedPictureToOpenGLUploader& );
};

#endif  //DECODEDPICTURETOOPENGLUPLOADER_H
