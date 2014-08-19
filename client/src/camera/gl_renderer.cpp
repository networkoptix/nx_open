#include "gl_renderer.h"

//#ifndef Q_OS_MACX
//#define GL_GLEXT_PROTOTYPES
//#include <GL/glext.h>
//#endif
//#define GL_GLEXT_PROTOTYPES 1
#include <QtGui/qopengl.h>

#include <cassert>

#include <QtCore/QCoreApplication> /* For Q_DECLARE_TR_FUNCTIONS. */
#include <QtCore/QScopedPointer>
#include <QtCore/QMutex>

#include <QtWidgets/QErrorMessage>

#include <utils/common/log.h>
#include <utils/common/warnings.h>
#include <utils/common/util.h>
#include <utils/media/sse_helper.h>
#include <opengl_renderer.h>
#include <utils/color_space/yuvconvert.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
#include <ui/graphics/items/resource/decodedpicturetoopengluploader.h>
#include <ui/common/geometry.h>
#include "ui/fisheye/fisheye_ptz_controller.h"

#include <camera/client_video_camera.h>

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
    
    
    const QString GAMMA_STRING(lit("clamp(pow(max(y+ yLevels2, 0.0) * yLevels1, yGamma), 0.0, 1.0)"));

    fisheyePtzProgram = new QnFisheyeRectilinearProgram(context, this);
    fisheyePtzGammaProgram =  new QnFisheyeRectilinearProgram(context, this, GAMMA_STRING);

    fisheyePanoHProgram = new QnFisheyeEquirectangularHProgram(context, this);
    fisheyePanoHGammaProgram = new QnFisheyeEquirectangularHProgram(context, this, GAMMA_STRING);

    fisheyePanoVProgram = new QnFisheyeEquirectangularVProgram(context, this);
    fisheyePanoVGammaProgram = new QnFisheyeEquirectangularVProgram(context, this, GAMMA_STRING);

    fisheyeRGBPtzProgram = new QnFisheyeRGBRectilinearProgram(context, this);
    fisheyeRGBPanoHProgram = new QnFisheyeRGBEquirectangularHProgram(context, this);
    fisheyeRGBPanoVProgram = new QnFisheyeRGBEquirectangularVProgram(context, this);
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
    QnGlFunctions(context),
    QOpenGLFunctions(context->contextHandle()),
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
    m_fisheyeController(0),

    m_initialized(false),
    m_positionBuffer(QOpenGLBuffer::VertexBuffer),
    m_textureBuffer(QOpenGLBuffer::VertexBuffer)

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
    NX_LOG( lit("Entered QnGLRenderer::paint"), cl_logDEBUG2 );

    QOpenGLFunctions::initializeOpenGLFunctions();

    DecodedPictureToOpenGLUploader::ScopedPictureLock picLock( m_decodedPictureProvider );
    if( !picLock.get() )
    {
        NX_LOG( lit("Exited QnGLRenderer::paint (1)"), cl_logDEBUG2 );
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
        const float v_array[] = { 
            (float)targetRect.left(), 
            (float)targetRect.top(),
            (float)targetRect.right(),
            (float)targetRect.top(), 
            (float)targetRect.right(), 
            (float)targetRect.bottom(), 
            (float)targetRect.left(), 
            (float)targetRect.bottom() 
        };
        switch( picLock->colorFormat() )
        {
            case PIX_FMT_RGBA:
                if (m_fisheyeController && m_fisheyeController->mediaDewarpingParams().enabled && m_fisheyeController->itemDewarpingParams().enabled)
                    drawFisheyeRGBVideoTexture(
                        picLock,
                        QnGeometry::subRect(picLock->textureRect(), sourceRect),
                        picLock->glTextures()[0],
                        v_array );
                else
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
                /*drawVideoTextureDirectly(
                    QnGeometry::subRect(picLock->textureRect(), sourceRect),
                    picLock->glTextures()[0],
                    v_array );*/
                
                Q_ASSERT( isYV12ToRgbShaderUsed() );
                drawYV12VideoTexture(
                    picLock,
                    QnGeometry::subRect(picLock->textureRect(), sourceRect),
                    picLock->glTextures()[0],
                    picLock->glTextures()[1],
                    picLock->glTextures()[2],
                    v_array,
                    picLock->flags() & QnAbstractMediaData::MediaFlags_StillImage);
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

    NX_LOG( lit("Exiting QnGLRenderer::paint (2)"), cl_logDEBUG2 );

    return result;
}

void QnGLRenderer::drawVideoTextureDirectly(
    const QRectF& tex0Coords,
    unsigned int tex0ID,
    const float* v_array )
{
    cl_log.log( lit("QnGLRenderer::drawVideoTextureDirectly. texture %1").arg(tex0ID), cl_logDEBUG2 );

    float tx_array[8] = {
        (float)tex0Coords.x(), (float)tex0Coords.y(),
        (float)tex0Coords.right(), (float)tex0Coords.top(),
        (float)tex0Coords.right(), (float)tex0Coords.bottom(),
        (float)tex0Coords.x(), (float)tex0Coords.bottom()
    };

    //Deprecated in OpenGL ES2.0
    //glEnable(GL_TEXTURE_2D);
    //DEBUG_CODE(glCheckError("glEnable"));

    glBindTexture(GL_TEXTURE_2D, tex0ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    QnOpenGLRendererManager::instance(QGLContext::currentContext()).setColor(QVector4D(1.0f,1.0f,1.0f,1.0f));
    QnOpenGLRendererManager::instance(QGLContext::currentContext()).drawBindedTextureOnQuad(v_array,tx_array);
    
//    glColor4f( 1, 1, 1, 1 );

    //drawBindedTexture( m_shaders->rgba, v_array, tx_array );
}

void QnGLRenderer::setScreenshotInterface(ScreenshotInterface* value) { 
    m_screenshotInterface = value;
}

ImageCorrectionResult QnGLRenderer::calcImageCorrection()
{
    if (m_screenshotInterface) {
        QImage img = m_screenshotInterface->getGrayscaleScreenshot();
        m_imageCorrector.analyseImage((quint8*)img.constBits(), img.width(), img.height(), img.bytesPerLine(), m_imgCorrectParam, m_displayedRect);
    }

    return m_imageCorrector;
}

void QnGLRenderer::drawYV12VideoTexture(
    const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
    const QRectF& tex0Coords,
    unsigned int tex0ID,
    unsigned int tex1ID,
    unsigned int tex2ID,
    const float* v_array,
    bool isStillImage)
{
    /*
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-10000,10000,10000,-10000,-10000,10000);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    */
    //QnOpenGLRendererManager::instance(QGLContext::currentContext()).getProjectionMatrix().setToIdentity();
    //QnOpenGLRendererManager::instance(QGLContext::currentContext()).getProjectionMatrix().ortho(-10000,10000,10000,-10000,-10000,10000);
    //QnOpenGLRendererManager::instance(QGLContext::currentContext()).getModelViewMatrix().setToIdentity();

    float tx_array[8] = {
        (float)tex0Coords.x(), (float)tex0Coords.y(),
        (float)tex0Coords.right(), (float)tex0Coords.top(),
        (float)tex0Coords.right(), (float)tex0Coords.bottom(),
        (float)tex0Coords.x(), (float)tex0Coords.bottom()
    };

    NX_LOG( lit("Rendering YUV420 textures %1, %2, %3").
        arg(tex0ID).arg(tex1ID).arg(tex2ID), cl_logDEBUG2 );

    //Deprecated in OpenGL ES2.0
    //glEnable(GL_TEXTURE_2D);
    //DEBUG_CODE(glCheckError("glEnable"));

    QnAbstractYv12ToRgbShaderProgram* shader;
    QnYv12ToRgbWithGammaShaderProgram* gammaShader = 0;
    QnFisheyeShaderProgram<QnYv12ToRgbWithGammaShaderProgram>* fisheyeShader = 0;
    QnMediaDewarpingParams mediaParams;
    QnItemDewarpingParams itemParams;

    float ar = 1.0;
    if (m_fisheyeController && m_fisheyeController->mediaDewarpingParams().enabled && m_fisheyeController->itemDewarpingParams().enabled)
    {
        ar = picLock->width()/(float)picLock->height();
        //m_fisheyeController->tick();
        mediaParams = m_fisheyeController->mediaDewarpingParams();
        itemParams = m_fisheyeController->itemDewarpingParams();

        if (itemParams.panoFactor > 1.0)
        {
            if (mediaParams.viewMode == QnMediaDewarpingParams::Horizontal)
            {
                if (m_imgCorrectParam.enabled)
                    gammaShader = fisheyeShader = m_shaders->fisheyePanoHGammaProgram;
                else
                    fisheyeShader = m_shaders->fisheyePanoHProgram;
            }
            else {
                if (m_imgCorrectParam.enabled)
                    gammaShader = fisheyeShader = m_shaders->fisheyePanoVGammaProgram;
                else
                    fisheyeShader = m_shaders->fisheyePanoVProgram;
            }
        }
        else {
            if (m_imgCorrectParam.enabled)
                gammaShader = fisheyeShader = m_shaders->fisheyePtzGammaProgram;
            else
                fisheyeShader = m_shaders->fisheyePtzProgram;
        }
        shader = fisheyeShader;
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
        fisheyeShader->setDewarpingParams(mediaParams, itemParams, ar, (float)tex0Coords.right(), (float)tex0Coords.bottom());
    }

    if (gammaShader) 
    {
        if (!isPaused() && !isStillImage) {
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
    glBindTexture(GL_TEXTURE_2D, tex2ID);
    DEBUG_CODE(glCheckError("glBindTexture"));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex1ID);
    DEBUG_CODE(glCheckError("glBindTexture"));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex0ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    drawBindedTexture( shader, v_array, tx_array );

    shader->release();
}

void QnGLRenderer::drawFisheyeRGBVideoTexture(
    const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
    const QRectF& tex0Coords,
    unsigned int tex0ID,
    const float* v_array)
{

    float tx_array[8] = {
        (float)tex0Coords.x(), (float)tex0Coords.y(),
        (float)tex0Coords.right(), (float)tex0Coords.top(),
        (float)tex0Coords.right(), (float)tex0Coords.bottom(),
        (float)tex0Coords.x(), (float)tex0Coords.bottom()
    };

    //Deprecated in OpenGL ES2.0
    //glEnable(GL_TEXTURE_2D);
    //DEBUG_CODE(glCheckError("glEnable"));

    QnFisheyeShaderProgram<QnAbstractRGBAShaderProgram>* fisheyeShader = 0;
    QnMediaDewarpingParams mediaParams;
    QnItemDewarpingParams itemParams;

    float ar = 1.0;
    ar = picLock->width()/(float)picLock->height();
    //m_fisheyeController->tick();
    mediaParams = m_fisheyeController->mediaDewarpingParams();
    itemParams = m_fisheyeController->itemDewarpingParams();

    if (itemParams.panoFactor > 1.0)
    {
        if (mediaParams.viewMode == QnMediaDewarpingParams::Horizontal)
            fisheyeShader = m_shaders->fisheyeRGBPanoHProgram;
        else
            fisheyeShader = m_shaders->fisheyeRGBPanoVProgram;
    }
    else {
        fisheyeShader = m_shaders->fisheyeRGBPtzProgram;
    }

    fisheyeShader->bind();
    fisheyeShader->setRGBATexture( 0 );
    fisheyeShader->setOpacity(m_decodedPictureProvider.opacity());

    fisheyeShader->setDewarpingParams(mediaParams, itemParams, ar, (float)tex0Coords.right(), (float)tex0Coords.bottom());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex0ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    drawBindedTexture( fisheyeShader, v_array, tx_array );

    fisheyeShader->release();
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
    Q_UNUSED(picLock)

    float tx_array[8] = {
        (float)tex0Coords.x(), (float)tex0Coords.y(),
        (float)tex0Coords.right(), (float)tex0Coords.top(),
        (float)tex0Coords.right(), (float)tex0Coords.bottom(),
        (float)tex0Coords.x(), (float)tex0Coords.bottom()
    };

    NX_LOG( lit("Rendering YUV420 textures %1, %2, %3").
        arg(tex0ID).arg(tex1ID).arg(tex2ID), cl_logDEBUG2 );
    //Deprecated in OpenGL ES2.0
    //glEnable(GL_TEXTURE_2D);
    //DEBUG_CODE(glCheckError("glEnable"));

    m_shaders->yv12ToRgba->bind();
    m_shaders->yv12ToRgba->setYTexture( 0 );
    m_shaders->yv12ToRgba->setUTexture( 1 );
    m_shaders->yv12ToRgba->setVTexture( 2 );
    m_shaders->yv12ToRgba->setATexture( 3 );
    m_shaders->yv12ToRgba->setOpacity(m_decodedPictureProvider.opacity() );

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, tex3ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex2ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex1ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex0ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    drawBindedTexture( m_shaders->yv12ToRgba, v_array, tx_array );

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

    //Deprecated in OpenGL ES2.0
    //glEnable(GL_TEXTURE_2D);
    //DEBUG_CODE(glCheckError("glEnable"));

    m_shaders->nv12ToRgb->bind();
    //m_shaders->nv12ToRgb->setParameters( m_brightness / 256.0f, m_contrast, m_hue, m_saturation, m_decodedPictureProvider.opacity() );
    m_shaders->nv12ToRgb->setYTexture( 0 );//yPlaneTexID );
    m_shaders->nv12ToRgb->setUVTexture( 1 );//uvPlaneTexID );
    m_shaders->nv12ToRgb->setOpacity( m_decodedPictureProvider.opacity() );
    m_shaders->nv12ToRgb->setColorTransform( QnNv12ToRgbShaderProgram::colorTransform(QnNv12ToRgbShaderProgram::YuvEbu) );

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, yPlaneTexID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, uvPlaneTexID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    drawBindedTexture( m_shaders->nv12ToRgb, v_array, tx_array );

    m_shaders->nv12ToRgb->release();
}


void QnGLRenderer::drawBindedTexture( QnGLShaderProgram* shader , const float* v_array, const float* tx_array ) {
    QByteArray data, texData;
    QnGlBufferStream<GLfloat> vertexStream(&data), texStream(&texData);

    for(int i = 0; i < 8; i++) {
        vertexStream << v_array[i];
        texStream << tx_array[i];
    }

    const int VERTEX_POS_INDX = 0;
    const int VERTEX_TEXCOORD0_INDX = 1;
    const int VERTEX_POS_SIZE = 2; // x, y
    const int VERTEX_TEXCOORD0_SIZE = 2; // s and t

    if (!m_initialized) {
        m_vertices.create();
        m_vertices.bind();

        m_positionBuffer.create();
        m_positionBuffer.setUsagePattern( QOpenGLBuffer::DynamicDraw );
        m_positionBuffer.bind();
        m_positionBuffer.allocate( data.data(), data.size() );
        shader->enableAttributeArray( VERTEX_POS_INDX );
        shader->setAttributeBuffer( VERTEX_POS_INDX, GL_FLOAT, 0, VERTEX_POS_SIZE );

        m_textureBuffer.create();
        m_textureBuffer.setUsagePattern( QOpenGLBuffer::DynamicDraw );
        m_textureBuffer.bind();
        m_textureBuffer.allocate( texData.data(), texData.size());
        shader->enableAttributeArray( VERTEX_TEXCOORD0_INDX );
        shader->setAttributeBuffer( VERTEX_TEXCOORD0_INDX, GL_FLOAT, 0, VERTEX_TEXCOORD0_SIZE );

        if (!shader->initialized()) {
            shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
            shader->bindAttributeLocation("aTexcoord",VERTEX_TEXCOORD0_INDX);
            shader->markInitialized();
        };    

        m_vertices.release();

        m_initialized = true;
    } else {
        m_positionBuffer.bind();
        m_positionBuffer.write(0, data.data(), data.size());
        m_positionBuffer.release();

        m_textureBuffer.bind();
        m_textureBuffer.write(0, texData.data(), texData.size());
        m_textureBuffer.release();

    }

    QnOpenGLRendererManager::instance(QGLContext::currentContext()).drawBindedTextureOnQuadVao(&m_vertices, shader);
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
    return !(features() & QnGlFunctions::ShadersBroken)
        && !m_decodedPictureProvider.isForcedSoftYUV()
        && m_shaders->yv12ToRgb
        && m_shaders->yv12ToRgb->wasLinked();
}

bool QnGLRenderer::isYV12ToRgbaShaderUsed() const
{
    return !(features() & QnGlFunctions::ShadersBroken)
        && !m_decodedPictureProvider.isForcedSoftYUV()
        && m_shaders->yv12ToRgba
        && m_shaders->yv12ToRgba->wasLinked();
}

bool QnGLRenderer::isNV12ToRgbShaderUsed() const
{
    return !(features() & QnGlFunctions::ShadersBroken)
        && !m_decodedPictureProvider.isForcedSoftYUV()
        && m_shaders->nv12ToRgb
        && m_shaders->nv12ToRgb->wasLinked();
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
    QMutexLocker lock(&m_mutex);
    m_fisheyeController = controller;
}

bool QnGLRenderer::isFisheyeEnabled() const
{
    QMutexLocker lock(&m_mutex);
    return m_fisheyeController && m_fisheyeController->getCapabilities() != Qn::NoCapabilities;
}
