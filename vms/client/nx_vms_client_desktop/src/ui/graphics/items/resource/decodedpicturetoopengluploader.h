// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QSharedPointer>
#include <deque>
#include <memory>
#include <set>
#include <vector>

#include <QtOpenGLWidgets/QOpenGLWidget>

#include <nx/media/media_data_packet.h> /* For QnMetaDataV1Ptr. */
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/stoppable.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/vms/api/data/image_correction_data.h>
#include <utils/common/safepool.h>
#include <utils/media/frame_info.h>

#include "utils/color_space/image_correction.h"

class QSurface;
class CLVideoDecoderOutput;
class DecodedPictureToOpenGLUploaderPrivate;
class QnGlRendererTexture;
class QnGlRendererTexturePack;
class AVPacketUploader;
class QQuickWidget;

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

    public:
        AVPixelFormat colorFormat() const;
        void setColorFormat(AVPixelFormat newFormat );
        int width() const;
        int height() const;
        QSize displaySize() const { return m_onScreenSize; }
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
        QnGlRendererTexturePack* texturePack();
        //!Returns opengl texture holding plane \a index data. Index of a plane depends on color format (Y, U, V for YV12; Y, UV for NV12 and RGB for rgb format)
        QnGlRendererTexture* texture( int index ) const;
        GLuint pboID( int index ) const;
        int flags() const;
        void processImage(
            quint8* yPlane,
            int width,
            int height,
            int stride,
            const nx::vms::api::ImageCorrectionData& data);

        const ImageCorrectionResult& imageCorrectionResult() const;

        /** Returns decoded frame if we don't user OpenGL textures (in case of RHI or software) */
        CLConstVideoDecoderOutputPtr decodedFrame() const { return m_decodedFrame; }

    private:
        struct PBOData
        {
            GLuint id;
            size_t sizeBytes;

            PBOData();
        };

        AVPixelFormat m_colorFormat;
        int m_width;
        int m_height;
        unsigned int m_sequence;
        quint64 m_pts;
        std::vector<PBOData> m_pbo;
        bool m_skippingForbidden;
        int m_flags;
        ImageCorrectionResult m_imgCorrection;
        QRectF m_displayedRect;
        QnGlRendererTexturePack* m_texturePack;
        QSize m_onScreenSize;
        CLConstVideoDecoderOutputPtr m_decodedFrame;

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

        \note If \a asyncDepth is 0 then it is possible that getUploadedPicture() will return nullptr (if the only gl buffer is currently used for uploading)
    */
    DecodedPictureToOpenGLUploader(
        QOpenGLWidget* glWidget,
        QQuickWidget* quickWidget,
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
        const CLConstVideoDecoderOutputPtr& decodedPicture,
        const QRectF displayedRect = QRectF(0.0, 0.0, 1.0, 1.0),
        const QSize& onScreenSize = QSize());
    //!Returns latest uploaded picture. Used by GUI thread
    /*!
        \return Uploaded picture data, nullptr if no picture. Returned object memory is managed by \a DecodedPictureToOpenGLUploader, and MUST NOT be deleted
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

    //!This method is called when upload is done
    void pictureDataUploadSucceeded(UploadedPicture* const picture);
    //!This method is called by asynchronous uploader when upload has failed
    void pictureDataUploadFailed(UploadedPicture* const picture);

    void setImageCorrection(const nx::vms::api::ImageCorrectionData& value);
    nx::vms::api::ImageCorrectionData getImageCorrection() const;

    //!Loads picture with dat stored in \a planes to opengl \a dest
    /*!
        \note Method is re-enterable
        \return false, if failed to make \a glContext current. Otherwise, true
    */
    bool uploadDataToGl(
        UploadedPicture* const dest, const CLConstVideoDecoderOutputPtr& frame);
    bool renderVideoMemory(
        UploadedPicture* const dest, const CLConstVideoDecoderOutputPtr& frame);
private:
    friend class QnGlRendererTexture;
    friend class DecodedPicturesDeleter;

    QSharedPointer<DecodedPictureToOpenGLUploaderPrivate> d;
    uchar* m_yuv2rgbBuffer;
    int m_yuv2rgbBufferLen;
    qreal m_painterOpacity;
    mutable nx::Mutex m_mutex;
    mutable nx::Mutex m_uploadMutex;
    mutable nx::WaitCondition m_cond;
    unsigned int m_previousPicSequence;
    mutable std::deque<UploadedPicture*> m_emptyBuffers;
    mutable std::deque<UploadedPicture*> m_renderedPictures;
    mutable std::deque<UploadedPicture*> m_picturesWaitingRendering;
    mutable std::deque<UploadedPicture*> m_picturesBeingRendered;
    mutable std::deque<AVPacketUploader*> m_framesWaitingUploadInGUIThread;
    bool m_terminated;
    quint8* m_rgbaBuf;
    int m_fileNumber;
    bool m_hardwareDecoderUsed;

    QOpenGLContext* m_initializedContext = nullptr;
    QSurface* m_initializedSurface = nullptr;

    nx::vms::api::ImageCorrectionData m_imageCorrection;

    bool usingShaderYuvToRgb() const;
    bool usingShaderNV12ToRgb() const;
    void releaseDecodedPicturePool( std::deque<UploadedPicture*>* const pool );
    //!Method is not thread-safe
    unsigned int nextPicSequenceValue();
    void releasePictureBuffers();
    void releasePictureBuffersNonSafe();
    void savePicToFile( AVFrame* const pic, int pts );
    //!m_mutex MUST be locked before this call
    void cancelUploadingInGUIThread();

    uchar* convertYuvToRgb(
        const AVPixelFormat format,
        const unsigned int width,
        const unsigned int height,
        const uint8_t* const planes[],
        const int lineSizes[]);

    DecodedPictureToOpenGLUploader( const DecodedPictureToOpenGLUploader& );
    DecodedPictureToOpenGLUploader& operator=( const DecodedPictureToOpenGLUploader& );
};
