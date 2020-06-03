#ifndef QN_GL_RENDERER_H
#define QN_GL_RENDERER_H

#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLBuffer>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLFramebufferObject>

#include <utils/media/frame_info.h>
#include <nx/streaming/media_data_packet.h> //< For QnMetaDataV1Ptr.

#include <client/client_globals.h>

#include <ui/graphics/opengl/gl_functions.h>

#include <ui/graphics/shaders/yv12_to_rgb_shader_program.h>
#include <ui/graphics/shaders/nv12_to_rgb_shader_program.h>


#include <ui/graphics/items/resource/decodedpicturetoopengluploader.h>
#include <ui/graphics/shaders/blur_shader_program.h>

class CLVideoDecoderOutput;
class ScreenshotInterface;
class QnHistogramConsumer;
class QnFisheyePtzController;

class QnGlRendererShaders: public QObject
{
    Q_OBJECT

public:
    QnGlRendererShaders(QObject* parent = nullptr);
    virtual ~QnGlRendererShaders();

    QnYv12ToRgbShaderProgram* const yv12ToRgb;
    QnYv12ToRgbWithGammaShaderProgram* const yv12ToRgbWithGamma;
    QnYv12ToRgbaShaderProgram* const yv12ToRgba;
    QnNv12ToRgbShaderProgram* const nv12ToRgb;
    QnBlurShaderProgram* const m_blurShader;

    struct FisheyeShaders
    {
        QnFisheyeRectilinearProgram* ptzProgram;
        QnFisheyeRectilinearProgram* ptzGammaProgram;

        QnFisheyeEquirectangularHProgram* panoHProgram;
        QnFisheyeEquirectangularHProgram* panoHGammaProgram;

        QnFisheyeEquirectangularVProgram* panoVProgram;
        QnFisheyeEquirectangularVProgram* panoVGammaProgram;

        QnFisheyeRGBRectilinearProgram* rgbPtzProgram;
        QnFisheyeRGBEquirectangularHProgram* rgbPanoHProgram;
        QnFisheyeRGBEquirectangularVProgram* rgbPanoVProgram;
    };

    const QHash<Qn::FisheyeCameraProjection, FisheyeShaders> fisheye;
};


class QnGLRenderer : protected QnGlFunctions, protected QOpenGLFunctions
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

    Qn::RenderStatus paint(const QRectF &sourceRect, const QRectF &targetRect);
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
    const DecodedPictureToOpenGLUploader& m_decodedPictureProvider;
    qreal m_brightness;
    qreal m_contrast;
    qreal m_hue;
    qreal m_saturation;
    qint64 m_lastDisplayedTime;
    unsigned m_lastDisplayedFlags;
    unsigned int m_prevFrameSequence;


    QSharedPointer<QnGlRendererShaders> m_shaders;
    bool m_timeChangeEnabled;
    mutable QnMutex m_mutex;
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

    //!Draws texture \a tex0ID with fisheye effect to the screen
    void drawFisheyeRGBVideoTexture(
        const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
        const QRectF& tex0Coords,
        unsigned int tex0ID,
        const float* v_array,
        qreal opacity);

    //!Draws to the screen YV12 image represented with three textures (one for each plane YUV) using shader which mixes all three planes to RGB
    void drawYV12VideoTexture(
        const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
        const QRectF& tex0Coords,
        unsigned int tex0ID,
        unsigned int tex1ID,
        unsigned int tex2ID,
        const float* v_array,
        bool isStillImage,
        qreal opacity);
    //!Draws YUV420 with alpha channel
    void drawYVA12VideoTexture(
        const DecodedPictureToOpenGLUploader::ScopedPictureLock& /*picLock*/,
        const QRectF& tex0Coords,
        unsigned int tex0ID,
        unsigned int tex1ID,
        unsigned int tex2ID,
        unsigned int tex3ID,
        const float* v_array,
        qreal opacity);
    //!Draws to the screen NV12 image represented with two textures (Y-plane and UV-plane) using shader which mixes both planes to RGB
    void drawNV12VideoTexture(
        const QRectF& tex0Coords,
        unsigned int tex0ID,
        unsigned int tex1ID,
        const float* v_array,
        qreal opacity);
    //!Draws currently binded texturere
    /*!
     * \param v_array
     * \param tx_array texture vertexes array
     */
    void drawBindedTexture( QnGLShaderProgram* shader , const float* v_array, const float* tx_array );

    Qn::RenderStatus drawVideoData(
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

	/* QOpenGLVertexArrayObject can be destroyed only in the current context.
	 * So if we are creating it on the stack, asynchronous deleting becomes unaccessible. */
    QSharedPointer<QOpenGLVertexArrayObject> m_vertices;
    QOpenGLBuffer m_positionBuffer;
    QOpenGLBuffer m_textureBuffer;

    std::unique_ptr<QOpenGLFramebufferObject> m_blurBufferA;
    std::unique_ptr<QOpenGLFramebufferObject> m_blurBufferB;
    qreal m_blurFactor;
    qreal m_prevBlurFactor;

    bool m_blurEnabled = true;
};

#endif //QN_GL_RENDERER_H
