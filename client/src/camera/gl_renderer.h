#ifndef QN_GL_RENDERER_H
#define QN_GL_RENDERER_H

#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QGLContext>

#include <utils/media/frame_info.h>
#include <core/datapacket/media_data_packet.h> /* For QnMetaDataV1Ptr. */

#include <client/client_globals.h>

#include <ui/graphics/opengl/gl_functions.h>

#include <ui/graphics/shaders/yv12_to_rgb_shader_program.h>
#include <ui/graphics/shaders/nv12_to_rgb_shader_program.h>


#include <ui/graphics/items/resource/decodedpicturetoopengluploader.h>

class CLVideoDecoderOutput;
class ScreenshotInterface;
class QnHistogramConsumer;
class QnFisheyePtzController;

class QnGlRendererShaders: public QObject {
    Q_OBJECT;
public:
    QnGlRendererShaders(QObject *parent = NULL);
    virtual ~QnGlRendererShaders();

    QnYv12ToRgbShaderProgram *yv12ToRgb;
    QnYv12ToRgbWithGammaShaderProgram *yv12ToRgbWithGamma;
    
    QnFisheyeRectilinearProgram* fisheyePtzProgram;
    QnFisheyeRectilinearProgram* fisheyePtzGammaProgram;

    QnFisheyeEquirectangularHProgram* fisheyePanoHProgram;
    QnFisheyeEquirectangularHProgram* fisheyePanoHGammaProgram;

    QnFisheyeEquirectangularVProgram* fisheyePanoVProgram;
    QnFisheyeEquirectangularVProgram* fisheyePanoVGammaProgram;

    QnFisheyeRGBRectilinearProgram* fisheyeRGBPtzProgram;
    QnFisheyeRGBEquirectangularHProgram* fisheyeRGBPanoHProgram;
    QnFisheyeRGBEquirectangularVProgram* fisheyeRGBPanoVProgram;

    QnYv12ToRgbaShaderProgram *yv12ToRgba;
    QnNv12ToRgbShaderProgram *nv12ToRgb;
};


class QnGLRenderer : protected QnGlFunctions, protected QOpenGLFunctions
{
public:
    static bool isPixelFormatSupported(PixelFormat pixfmt);

    QnGLRenderer( const QGLContext* context, const DecodedPictureToOpenGLUploader& decodedPictureProvider );
    ~QnGLRenderer();

    /*!
        Called with corresponding QGLContext is surely alive
    */
    void beforeDestroy();
    
    Qn::RenderStatus paint(const QRectF &sourceRect, const QRectF &targetRect);

    bool isLowQualityImage() const;
    
    qint64 lastDisplayedTime() const;

    QnMetaDataV1Ptr lastFrameMetadata() const; 
    bool isHardwareDecoderUsed() const;

    bool isYV12ToRgbShaderUsed() const;
    bool isYV12ToRgbaShaderUsed() const;
    bool isNV12ToRgbShaderUsed() const;

    void setImageCorrectionParams(const ImageCorrectionParams& params) { m_imgCorrectParam = params; }
    void setFisheyeController(QnFisheyePtzController* controller);
    bool isFisheyeEnabled() const;
    ImageCorrectionParams getImageCorrectionParams() const { return m_imgCorrectParam; }
    
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
    QnMetaDataV1Ptr m_lastDisplayedMetadata; // TODO: #Elric get rid of this
    unsigned m_lastDisplayedFlags;
    unsigned int m_prevFrameSequence;


    QSharedPointer<QnGlRendererShaders> m_shaders;
    bool m_timeChangeEnabled;
    mutable QnMutex m_mutex;
    bool m_paused;
    ScreenshotInterface* m_screenshotInterface;
    ImageCorrectionResult m_imageCorrector;
    ImageCorrectionParams m_imgCorrectParam;
    QnHistogramConsumer* m_histogramConsumer;
    QnFisheyePtzController* m_fisheyeController;
    QRectF m_displayedRect;
    
    void update( const QSharedPointer<CLVideoDecoderOutput>& curImg );
    //!Draws texture \a tex0ID to the screen
    void drawVideoTextureDirectly(
        const QRectF& tex0Coords,
        unsigned int tex0ID,
        const float* v_array );

    //!Draws texture \a tex0ID with fisheye effect to the screen
    void drawFisheyeRGBVideoTexture(
        const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
        const QRectF& tex0Coords,
        unsigned int tex0ID,
        const float* v_array);

    //!Draws to the screen YV12 image represented with three textures (one for each plane YUV) using shader which mixes all three planes to RGB
    void drawYV12VideoTexture(
        const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
        const QRectF& tex0Coords,
        unsigned int tex0ID,
        unsigned int tex1ID,
        unsigned int tex2ID,
        const float* v_array,
        bool isStillImage);
    //!Draws YUV420 with alpha channel
    void drawYVA12VideoTexture(
        const DecodedPictureToOpenGLUploader::ScopedPictureLock& /*picLock*/,
        const QRectF& tex0Coords,
        unsigned int tex0ID,
        unsigned int tex1ID,
        unsigned int tex2ID,
        unsigned int tex3ID,
        const float* v_array );
    //!Draws to the screen NV12 image represented with two textures (Y-plane and UV-plane) using shader which mixes both planes to RGB
    void drawNV12VideoTexture(
        const QRectF& tex0Coords,
        unsigned int tex0ID,
        unsigned int tex1ID,
        const float* v_array );
    //!Draws currently binded texturere
    /*!
     * \param v_array
     * \param tx_array texture vertexes array
     */
    void drawBindedTexture( QnGLShaderProgram* shader , const float* v_array, const float* tx_array );
    void updateTexture( const QSharedPointer<CLVideoDecoderOutput>& curImg );
    bool isYuvFormat() const;
    int glRGBFormat() const;

private:
    bool m_initialized;

	/* QOpenGLVertexArrayObject can be destroyed only in the current context.
	 * So if we are creating it on the stack, asynchronous deleting becomes unaccessible. */
    QSharedPointer<QOpenGLVertexArrayObject> m_vertices;
    QOpenGLBuffer m_positionBuffer;
    QOpenGLBuffer m_textureBuffer;
};

#endif //QN_GL_RENDERER_H
