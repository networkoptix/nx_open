// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_GL_RENDERER_H
#define QN_GL_RENDERER_H

#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLFramebufferObject>
#include <QtOpenGL/QOpenGLVertexArrayObject>

#include <client/client_globals.h>
#include <nx/media/media_data_packet.h> //< For QnMetaDataV1Ptr.
#include <ui/graphics/items/resource/decodedpicturetoopengluploader.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <utils/media/frame_info.h>

class CLVideoDecoderOutput;
class ScreenshotInterface;
class QnHistogramConsumer;
class QnFisheyePtzController;
class QnGLShaderProgram;

namespace nx::vms::client::desktop { class RhiVideoRenderer; }

class QnGLRenderer : protected QnGlFunctions
{
public:
    static bool isPixelFormatSupported(AVPixelFormat pixfmt);

    QnGLRenderer(QOpenGLWidget* glWidget, const DecodedPictureToOpenGLUploader& decodedPictureProvider );
    ~QnGLRenderer();

    /*!
        Called with corresponding QOpenGLContext is surely alive
    */
    void beforeDestroy();

    bool isBlurEnabled() const;
    void setBlurEnabled(bool value);

    /**
     * Set blur in range [0..1]
     */
    void setBlurFactor(qreal value);

    Qn::RenderStatus paint(QPainter* painter, const QRectF &sourceRect, const QRectF &targetRect);
    /** The same as paint but don't do actual painting. */
    Qn::RenderStatus discardFrame();

    bool isLowQualityImage() const;

    qint64 lastDisplayedTime() const;

    bool isHardwareDecoderUsed() const;

    bool isYV12ToRgbShaderUsed() const;
    bool isYV12ToRgbaShaderUsed() const;
    bool isNV12ToRgbShaderUsed() const;

    void setImageCorrectionParams(const nx::vms::api::ImageCorrectionData& params) { m_imgCorrectParam = params; }
    void setFisheyeController(QnFisheyePtzController* controller);
    bool isFisheyeEnabled() const;
    nx::vms::api::ImageCorrectionData getImageCorrectionParams() const { return m_imgCorrectParam; }

    void setPaused(bool value) { m_paused = value; }
    bool isPaused() const { return m_paused; }
    void setScreenshotInterface(ScreenshotInterface* value);
    void setDisplayedRect(const QRectF& rect);

    void setHistogramConsumer(QnHistogramConsumer* value);

private:
    void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation); // deprecated
    ImageCorrectionResult calcImageCorrection();
private:
    QOpenGLFunctions* const m_gl;
    const DecodedPictureToOpenGLUploader& m_decodedPictureProvider;
    qreal m_brightness;
    qreal m_contrast;
    qreal m_hue;
    qreal m_saturation;
    qint64 m_lastDisplayedTime;
    unsigned m_lastDisplayedFlags;
    unsigned int m_prevFrameSequence;

    bool m_timeChangeEnabled;
    mutable nx::Mutex m_mutex;
    bool m_paused;
    ScreenshotInterface* m_screenshotInterface;
    ImageCorrectionResult m_imageCorrector;
    nx::vms::api::ImageCorrectionData m_imgCorrectParam;
    QnHistogramConsumer* m_histogramConsumer;
    QnFisheyePtzController* m_fisheyeController;
    QRectF m_displayedRect;

    //!Draws texture \a tex0ID to the screen
    void drawVideoTextureDirectly(
        const QRectF& tex0Coords,
        unsigned int tex0ID,
        const float* v_array,
        qreal opacity);

    //!Draws to the screen an image represented with one (RGBA), two (NV12 - Y and UV),
    // three (YV12 - Y, U and V) or four (YUVA12 - Y, U, V and A) textures using shader,
    // applying fisheye dewarping if needed.
    bool drawVideoTextures(
        const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
        QPainter* painter,
        const QRectF& textureRect,
        const QRectF& viewRect,
        int planeCount,
        const unsigned int* textureIds,
        bool isStillImage,
        qreal opacity);

    //!Draws currently binded texture
    /*!
     * \param v_array
     * \param tx_array texture vertexes array
     */
    void drawBindedTexture(QnGLShaderProgram* shader , const float* v_array, const float* tx_array);

    Qn::RenderStatus drawVideoData(
        QPainter* painter,
        const QRectF &sourceRect,
        const QRectF &targetRect,
        qreal opacity);

    Qn::RenderStatus prepareBlurBuffers();
    Qn::RenderStatus renderBlurFBO(const QRectF &sourceRect);
    void doBlurStep(
        const QRectF& sourceRect,
        const QRectF& dstRect,
        GLuint texture,
        const QVector2D& textureOffset,
        bool isHorizontalPass);

private:
    bool m_initialized;

    // QOpenGLVertexArrayObject can be destroyed only in the current context.
    // So if we are creating it on the stack, asynchronous deleting becomes unaccessible.
    QSharedPointer<QOpenGLVertexArrayObject> m_vertices;
    QOpenGLBuffer m_positionBuffer;
    QOpenGLBuffer m_textureBuffer;

    std::unique_ptr<QOpenGLFramebufferObject> m_blurBufferA;
    std::unique_ptr<QOpenGLFramebufferObject> m_blurBufferB;
    qreal m_blurFactor;
    qreal m_prevBlurFactor;

    bool m_blurEnabled = true;
    std::shared_ptr<nx::vms::client::desktop::RhiVideoRenderer> m_rhiVideoRenderer;
};

#endif //QN_GL_RENDERER_H
