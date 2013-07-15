
#ifndef __APPLE__
#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>
#endif

#include "gl_renderer.h"

#include <cassert>

#include <QtCore/QCoreApplication> /* For Q_DECLARE_TR_FUNCTIONS. */
#include <QtCore/QScopedPointer>
#include <QtCore/QMutex>
#include <QtGui/QErrorMessage>

#include <utils/common/warnings.h>
#include <utils/common/util.h>
#include <utils/media/sse_helper.h>

#include <utils/color_space/yuvconvert.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/items/resource/decodedpicturetoopengluploader.h>
#include <ui/common/geometry.h>
#include "ui/fisheye/fisheye_ptz_processor.h"

#include "video_camera.h"

#ifdef QN_GL_RENDERER_DEBUG_PERFORMANCE
#   include <utils/common/performance.h>
#endif
#include "utils/color_space/image_correction.h"

/**
 * \def QN_GL_RENDERER_DEBUG
 * 
 * Enable OpenGL error reporting. Note that this will result in a LOT of
 * redundant <tt>glGetError</tt> calls, which may affect performance.
 */
//#define QN_GL_RENDERER_DEBUG

#ifdef QN_GL_RENDERER_DEBUG
#   define glCheckError glCheckError
#else
#   define glCheckError(...)
#endif

namespace {
    const int ROUND_COEFF = 8;

    int minPow2(int value)
    {
        int result = 1;
        while (value > result)
            result <<= 1;

        return result;
    }

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnGlRendererShaders
// -------------------------------------------------------------------------- //
QnGlRendererShaders::QnGlRendererShaders(const QGLContext *context, QObject *parent): QObject(parent) {
    yv12ToRgb = new QnYv12ToRgbShaderProgram(context, this);
    yv12ToRgbWithGamma = new QnYv12ToRgbWithGammaShaderProgram(context, this);
    yv12ToRgba = new QnYv12ToRgbaShaderProgram(context, this);
    nv12ToRgb = new QnNv12ToRgbShaderProgram(context, this);
    fisheyeProgram = new QnFisheyeShaderProgram(context, this);
    fisheyeGammaProgram = new QnFisheyeWithGammaShaderProgram(context, this);
}

QnGlRendererShaders::~QnGlRendererShaders() {
    return;
}

Q_GLOBAL_STATIC(QnGlContextData<QnGlRendererShaders>, qn_glRendererShaders_instanceStorage);


// -------------------------------------------------------------------------- //
// QnGLRenderer
// -------------------------------------------------------------------------- //
bool QnGLRenderer::isPixelFormatSupported( PixelFormat pixfmt )
{
    switch( pixfmt )
    {
        case PIX_FMT_YUV422P:
        case PIX_FMT_YUV420P:
        case PIX_FMT_YUV444P:
        case PIX_FMT_RGBA:
        case PIX_FMT_BGRA:
        case PIX_FMT_RGB24:
        case PIX_FMT_BGR24:
            return true;

        default:
            return false;
    }
}

QnGLRenderer::QnGLRenderer( const QGLContext* context, const DecodedPictureToOpenGLUploader& decodedPictureProvider ):
    QnGlFunctions( context ),
    m_decodedPictureProvider( decodedPictureProvider ),
    m_brightness( 0 ),
    m_contrast( 0 ),
    m_hue( 0 ),
    m_saturation( 0 ),
    m_lastDisplayedTime( AV_NOPTS_VALUE ),
    m_lastDisplayedFlags( 0 ),
    m_prevFrameSequence( 0 ),
    m_timeChangeEnabled(true),
    m_paused(false),
    m_screenshotInterface(0),
    m_histogramConsumer(0),
    m_fisheyeController(0)

    //m_extraMin(-PI/4.0),
    //m_extraMax(PI/4.0) // rotation range
{
    Q_ASSERT( context );

    applyMixerSettings( m_brightness, m_contrast, m_hue, m_saturation );
    
    m_shaders = qn_glRendererShaders_instanceStorage()->get(context);
    


    cl_log.log( QString(QLatin1String("OpenGL max texture size: %1.")).arg(QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE)), cl_logINFO );
}

QnGLRenderer::~QnGLRenderer()
{
}

void QnGLRenderer::beforeDestroy()
{
    m_shaders.clear();
}

void QnGLRenderer::applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation)
{
    // normalize the values
    m_brightness = brightness * 128;
    m_contrast = contrast + 1.0;
    m_hue = hue * 180.;
    m_saturation = saturation + 1.0;
}

Qn::RenderStatus QnGLRenderer::paint(const QRectF &sourceRect, const QRectF &targetRect)
{
    NX_LOG( QString::fromLatin1("Entered QnGLRenderer::paint"), cl_logDEBUG2 );

    DecodedPictureToOpenGLUploader::ScopedPictureLock picLock( m_decodedPictureProvider );
    if( !picLock.get() )
    {
        NX_LOG( QString::fromLatin1("Exited QnGLRenderer::paint (1)"), cl_logDEBUG2 );
        return Qn::NothingRendered;
    }

    m_lastDisplayedFlags = picLock->flags();

    int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    Qn::RenderStatus result = picLock->sequence() != m_prevFrameSequence ? Qn::NewFrameRendered : Qn::OldFrameRendered;
    const bool draw = picLock->width() <= maxTextureSize && picLock->height() <= maxTextureSize;
    if( !draw )
    {
        result = Qn::CannotRender;
    } 
    else if( picLock->width() > 0 && picLock->height() > 0 )
    {
        const float v_array[] = { (float)targetRect.left(), (float)targetRect.top(), (float)targetRect.right(), (float)targetRect.top(), (float)targetRect.right(), (float)targetRect.bottom(), (float)targetRect.left(), (float)targetRect.bottom() };
        switch( picLock->colorFormat() )
        {
            case PIX_FMT_RGBA:
                drawVideoTextureDirectly(
                    QnGeometry::subRect(picLock->textureRect(), sourceRect),
                    picLock->glTextures()[0],
                    v_array );
                break;

            case PIX_FMT_YUVA420P:
                Q_ASSERT( isYV12ToRgbaShaderUsed() );
                drawYVA12VideoTexture(
                    picLock,
                    QnGeometry::subRect(picLock->textureRect(), sourceRect),
                    picLock->glTextures()[0],
                    picLock->glTextures()[1],
                    picLock->glTextures()[2],
                    picLock->glTextures()[3],
                    v_array );
                break;

            case PIX_FMT_YUV420P:
                Q_ASSERT( isYV12ToRgbShaderUsed() );
                drawYV12VideoTexture(
                    picLock,
                    QnGeometry::subRect(picLock->textureRect(), sourceRect),
                    picLock->glTextures()[0],
                    picLock->glTextures()[1],
                    picLock->glTextures()[2],
                    v_array );
                break;

            case PIX_FMT_NV12:
                Q_ASSERT( isNV12ToRgbShaderUsed() );
                drawNV12VideoTexture(
                    QnGeometry::subRect(picLock->textureRect(), sourceRect),
                    picLock->glTextures()[0],
                    picLock->glTextures()[1],
                    v_array );
                break;

            default:
                //other formats must be converted to PIX_FMT_YUV420P or PIX_FMT_NV12 before picture uploading
                Q_ASSERT( false );
        }

        QMutexLocker lock(&m_mutex);

        if( picLock->pts() != AV_NOPTS_VALUE && m_prevFrameSequence != picLock->sequence()) 
        {
            if (m_timeChangeEnabled) {
                m_lastDisplayedTime = picLock->pts();
                //qDebug() << "QnGLRenderer::paint. Frame timestamp ("<<m_lastDisplayedTime<<") " <<
                //    QDateTime::fromMSecsSinceEpoch(m_lastDisplayedTime/1000).toString(QLatin1String("hh:mm:ss.zzz"));
            }
        }
        m_prevFrameSequence = picLock->sequence();
        m_lastDisplayedMetadata = picLock->metadata();
    }
    else
    {
        result = Qn::NothingRendered;
    }

    NX_LOG( QString::fromLatin1("Exiting QnGLRenderer::paint (2)"), cl_logDEBUG2 );

    return result;
}

void QnGLRenderer::drawVideoTextureDirectly(
    const QRectF& tex0Coords,
    unsigned int tex0ID,
    const float* v_array )
{
    cl_log.log( QString::fromAscii("QnGLRenderer::drawVideoTextureDirectly. texture %1").arg(tex0ID), cl_logDEBUG2 );

    float tx_array[8] = {
        (float)tex0Coords.x(), (float)tex0Coords.y(),
        (float)tex0Coords.right(), (float)tex0Coords.top(),
        (float)tex0Coords.right(), (float)tex0Coords.bottom(),
        (float)tex0Coords.x(), (float)tex0Coords.bottom()
    };

    glEnable(GL_TEXTURE_2D);
    DEBUG_CODE(glCheckError("glEnable"));

    glBindTexture(GL_TEXTURE_2D, tex0ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    drawBindedTexture( v_array, tx_array );
}

void QnGLRenderer::setScreenshotInterface(ScreenshotInterface* value) { 
    m_screenshotInterface = value;
}

ImageCorrectionResult QnGLRenderer::calcImageCorrection()
{
    if (m_screenshotInterface) {
        QImage img = m_screenshotInterface->getGrayscaleScreenshot();
        m_imageCorrector.analizeImage((quint8*)img.constBits(), img.width(), img.height(), img.bytesPerLine(), m_imgCorrectParam, m_displayedRect);
    }

    return m_imageCorrector;
}

void QnGLRenderer::drawYV12VideoTexture(
    const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
    const QRectF& tex0Coords,
    unsigned int tex0ID,
    unsigned int tex1ID,
    unsigned int tex2ID,
    const float* v_array )
{
    /*
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-10000,10000,10000,-10000,-10000,10000);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    */

    float tx_array[8] = {
        (float)tex0Coords.x(), (float)tex0Coords.y(),
        (float)tex0Coords.right(), (float)tex0Coords.top(),
        (float)tex0Coords.right(), (float)tex0Coords.bottom(),
        (float)tex0Coords.x(), (float)tex0Coords.bottom()
    };

    NX_LOG( QString::fromLatin1("Rendering YUV420 textures %1, %2, %3").
        arg(tex0ID).arg(tex1ID).arg(tex2ID), cl_logDEBUG2 );

    glEnable(GL_TEXTURE_2D);
    DEBUG_CODE(glCheckError("glEnable"));

    QnAbstractYv12ToRgbShaderProgram* shader;
    QnYv12ToRgbWithGammaShaderProgram* gammaShader = 0;
    QnFisheyeShaderProgram* fisheyeShader = 0;
    DevorpingParams params;
    if (m_fisheyeController && m_fisheyeController->isEnabled()) 
    {
        params = m_fisheyeController->getDevorpingParams();
        if (m_imgCorrectParam.enabled)
            shader = gammaShader = fisheyeShader = m_shaders->fisheyeGammaProgram;
        else
            shader = fisheyeShader = m_shaders->fisheyeProgram;
    }
    else if (m_imgCorrectParam.enabled) {
        shader = gammaShader = m_shaders->yv12ToRgbWithGamma;
    }
    else {
        shader = m_shaders->yv12ToRgb;
    }
    shader->bind();
    shader->setYTexture( 0 );
    shader->setUTexture( 1 );
    shader->setVTexture( 2 );
    shader->setOpacity(m_decodedPictureProvider.opacity());

    if (fisheyeShader) {
        params.aspectRatio = picLock->width()/(float)picLock->height();
        fisheyeShader->setDevorpingParams(params);
    }
    //shader->setDstFov(m_extraCurValue);
    //qDebug() << "m_extraCurValue" << m_extraCurValue;


    if (gammaShader) 
    {
        if (!isPaused()) {
            gammaShader->setImageCorrection(picLock->imageCorrectionResult());
            if (m_histogramConsumer)
                m_histogramConsumer->setHistogramData(picLock->imageCorrectionResult());
        }
        else {
            gammaShader->setImageCorrection(calcImageCorrection());
            if (m_histogramConsumer) 
                m_histogramConsumer->setHistogramData(m_imageCorrector);
        }
    }

    glActiveTexture(GL_TEXTURE2);
    DEBUG_CODE(glCheckError("glActiveTexture"));
    glBindTexture(GL_TEXTURE_2D, tex2ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    glActiveTexture(GL_TEXTURE1);
    DEBUG_CODE(glCheckError("glActiveTexture"));
    glBindTexture(GL_TEXTURE_2D, tex1ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    glActiveTexture(GL_TEXTURE0);
    DEBUG_CODE(glCheckError("glActiveTexture"));
    glBindTexture(GL_TEXTURE_2D, tex0ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    drawBindedTexture( v_array, tx_array );

    shader->release();
}

#ifndef GL_TEXTURE3
#   define GL_TEXTURE3                          0x84C3
#endif

void QnGLRenderer::drawYVA12VideoTexture(
    const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
    const QRectF& tex0Coords,
    unsigned int tex0ID,
    unsigned int tex1ID,
    unsigned int tex2ID,
    unsigned int tex3ID,
    const float* v_array )
{
    float tx_array[8] = {
        (float)tex0Coords.x(), (float)tex0Coords.y(),
        (float)tex0Coords.right(), (float)tex0Coords.top(),
        (float)tex0Coords.right(), (float)tex0Coords.bottom(),
        (float)tex0Coords.x(), (float)tex0Coords.bottom()
    };

    NX_LOG( QString::fromLatin1("Rendering YUV420 textures %1, %2, %3").
        arg(tex0ID).arg(tex1ID).arg(tex2ID), cl_logDEBUG2 );

    glEnable(GL_TEXTURE_2D);
    DEBUG_CODE(glCheckError("glEnable"));

    m_shaders->yv12ToRgba->bind();
    m_shaders->yv12ToRgba->setYTexture( 0 );
    m_shaders->yv12ToRgba->setUTexture( 1 );
    m_shaders->yv12ToRgba->setVTexture( 2 );
    m_shaders->yv12ToRgba->setATexture( 3 );
    m_shaders->yv12ToRgba->setOpacity(m_decodedPictureProvider.opacity() );

    glActiveTexture(GL_TEXTURE3);
    DEBUG_CODE(glCheckError("glActiveTexture"));
    glBindTexture(GL_TEXTURE_2D, tex3ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    glActiveTexture(GL_TEXTURE2);
    DEBUG_CODE(glCheckError("glActiveTexture"));
    glBindTexture(GL_TEXTURE_2D, tex2ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    glActiveTexture(GL_TEXTURE1);
    DEBUG_CODE(glCheckError("glActiveTexture"));
    glBindTexture(GL_TEXTURE_2D, tex1ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    glActiveTexture(GL_TEXTURE0);
    DEBUG_CODE(glCheckError("glActiveTexture"));
    glBindTexture(GL_TEXTURE_2D, tex0ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    drawBindedTexture( v_array, tx_array );

    m_shaders->yv12ToRgba->release();
}

void QnGLRenderer::drawNV12VideoTexture(
    const QRectF& tex0Coords,
    unsigned int yPlaneTexID,
    unsigned int uvPlaneTexID,
    const float* v_array )
{
    float tx_array[8] = {
        0.0f, 0.0f,
        (float)tex0Coords.x(), 0.0f,
        (float)tex0Coords.x(), (float)tex0Coords.y(),
        0.0f, (float)tex0Coords.y()
    };

    glEnable(GL_TEXTURE_2D);
    DEBUG_CODE(glCheckError("glEnable"));

    m_shaders->nv12ToRgb->bind();
    //m_shaders->nv12ToRgb->setParameters( m_brightness / 256.0f, m_contrast, m_hue, m_saturation, m_decodedPictureProvider.opacity() );
    m_shaders->nv12ToRgb->setYTexture( yPlaneTexID );
    m_shaders->nv12ToRgb->setUVTexture( uvPlaneTexID );
    m_shaders->nv12ToRgb->setOpacity( m_decodedPictureProvider.opacity() );
    m_shaders->nv12ToRgb->setColorTransform( QnNv12ToRgbShaderProgram::colorTransform(QnNv12ToRgbShaderProgram::YuvEbu) );

    glActiveTexture(GL_TEXTURE1);
    DEBUG_CODE(glCheckError("glActiveTexture"));
    glBindTexture(GL_TEXTURE_2D, yPlaneTexID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    glActiveTexture(GL_TEXTURE0);
    DEBUG_CODE(glCheckError("glActiveTexture"));
    glBindTexture(GL_TEXTURE_2D, uvPlaneTexID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    drawBindedTexture( v_array, tx_array );

    m_shaders->nv12ToRgb->release();
}

void QnGLRenderer::drawBindedTexture( const float* v_array, const float* tx_array )
{
    DEBUG_CODE(glCheckError("glBindBuffer"));
    glVertexPointer(2, GL_FLOAT, 0, v_array);
    DEBUG_CODE(glCheckError("glVertexPointer"));
    glTexCoordPointer(2, GL_FLOAT, 0, tx_array);
    DEBUG_CODE(glCheckError("glTexCoordPointer"));
    glEnableClientState(GL_VERTEX_ARRAY);
    DEBUG_CODE(glCheckError("glEnableClientState"));
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    DEBUG_CODE(glCheckError("glEnableClientState"));
    glDrawArrays(GL_QUADS, 0, 4);
    DEBUG_CODE(glCheckError("glDrawArrays"));
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    DEBUG_CODE(glCheckError("glDisableClientState"));
    glDisableClientState(GL_VERTEX_ARRAY);
    DEBUG_CODE(glCheckError("glDisableClientState"));
}

qint64 QnGLRenderer::lastDisplayedTime() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastDisplayedTime;
}

bool QnGLRenderer::isLowQualityImage() const
{
    return m_lastDisplayedFlags & QnAbstractMediaData::MediaFlags_LowQuality;
}

QnMetaDataV1Ptr QnGLRenderer::lastFrameMetadata() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastDisplayedMetadata;
}

bool QnGLRenderer::isHardwareDecoderUsed() const
{
    return m_lastDisplayedFlags & QnAbstractMediaData::MediaFlags_HWDecodingUsed;
}

bool QnGLRenderer::isYV12ToRgbShaderUsed() const
{
    return (features() & QnGlFunctions::ArbPrograms)
        && (features() & QnGlFunctions::OpenGL1_3)
        && !(features() & QnGlFunctions::ShadersBroken)
        && !m_decodedPictureProvider.isForcedSoftYUV()
        && m_shaders->yv12ToRgb
        && m_shaders->yv12ToRgb->isLinked();
}

bool QnGLRenderer::isYV12ToRgbaShaderUsed() const
{
    return (features() & QnGlFunctions::ArbPrograms)
        && (features() & QnGlFunctions::OpenGL1_3)
        && !(features() & QnGlFunctions::ShadersBroken)
        && !m_decodedPictureProvider.isForcedSoftYUV()
        && m_shaders->yv12ToRgba
        && m_shaders->yv12ToRgba->isLinked();
}

bool QnGLRenderer::isNV12ToRgbShaderUsed() const
{
    return (features() & QnGlFunctions::ArbPrograms)
        && (features() & QnGlFunctions::OpenGL1_3)
        && !(features() & QnGlFunctions::ShadersBroken)
        && !m_decodedPictureProvider.isForcedSoftYUV()
        && m_shaders->nv12ToRgb
        /*&& m_shaders->nv12ToRgb->isLinked()*/;
}

void QnGLRenderer::setDisplayedRect(const QRectF& rect)
{
    m_displayedRect = rect;
}

void QnGLRenderer::setHistogramConsumer(QnHistogramConsumer* value) 
{ 
    m_histogramConsumer = value; 
}

void QnGLRenderer::setFisheyeController(QnFisheyePtzController* controller)
{
    m_fisheyeController = controller;
}
