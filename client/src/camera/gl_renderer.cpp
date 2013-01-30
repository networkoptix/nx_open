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

#include "video_camera.h"
#include "../ui/graphics/items/resource/decodedpicturetoopengluploader.h"

#ifdef QN_GL_RENDERER_DEBUG_PERFORMANCE
#   include <utils/common/performance.h>
#endif

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
// QnGLRenderer
// -------------------------------------------------------------------------- //
static int maxTextureSizeVal = 0;

int QnGLRenderer::maxTextureSize() 
{
    return maxTextureSizeVal;
}

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

QnGLRenderer::QnGLRenderer( const QGLContext* context, const DecodedPictureToOpenGLUploader& decodedPictureProvider )
:
    QnGlFunctions( context ),
    m_decodedPictureProvider( decodedPictureProvider ),
    m_brightness( 0 ),
    m_contrast( 0 ),
    m_hue( 0 ),
    m_saturation( 0 ),
    m_lastDisplayedTime( AV_NOPTS_VALUE ),
    m_lastDisplayedFlags( 0 ),
    m_prevFrameSequence( 0 ),
    m_timeChangeEnabled(true)
{
    Q_ASSERT( context );

    applyMixerSettings( m_brightness, m_contrast, m_hue, m_saturation );
    /* Prepare shaders. */
    m_yuy2ToRgbShaderProgram.reset( new QnYuy2ToRgbShaderProgram(context) );
    m_yv12ToRgbShaderProgram.reset( new QnYv12ToRgbShaderProgram(context) );
    //m_nv12ToRgbShaderProgram.reset( new QnNv12ToRgbShaderProgram(context) );

    glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTextureSizeVal );
    cl_log.log( QString(QLatin1String("OpenGL max texture size: %1.")).arg(maxTextureSizeVal), cl_logINFO );
}

QnGLRenderer::~QnGLRenderer()
{
}

void QnGLRenderer::beforeDestroy()
{
    m_yuy2ToRgbShaderProgram.reset();
    m_yv12ToRgbShaderProgram.reset();
    m_nv12ToRgbShaderProgram.reset();
}

void QnGLRenderer::applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation)
{
    // normalize the values
    m_brightness = brightness * 128;
    m_contrast = contrast + 1.0;
    m_hue = hue * 180.;
    m_saturation = saturation + 1.0;
}

Qn::RenderStatus QnGLRenderer::paint( const QRectF& r )
{
    NX_LOG( QString::fromLatin1("Entered QnGLRenderer::paint"), cl_logDEBUG2 );

    DecodedPictureToOpenGLUploader::ScopedPictureLock picLock( m_decodedPictureProvider );
    if( !picLock.get() )
    {
        NX_LOG( QString::fromLatin1("Exited QnGLRenderer::paint (1)"), cl_logDEBUG2 );
        return Qn::NothingRendered;
    }

    m_lastDisplayedFlags = picLock->flags();

    Qn::RenderStatus result = picLock->sequence() != m_prevFrameSequence ? Qn::NewFrameRendered : Qn::OldFrameRendered;
    const bool draw = picLock->width() <= maxTextureSize() && picLock->height() <= maxTextureSize();
    if( !draw )
    {
        result = Qn::CannotRender;
    } 
    else if( picLock->width() > 0 && picLock->height() > 0 )
    {
        const float v_array[] = { (float)r.left(), (float)r.top(), (float)r.right(), (float)r.top(), (float)r.right(), (float)r.bottom(), (float)r.left(), (float)r.bottom() };
        switch( picLock->colorFormat() )
        {
            case PIX_FMT_RGBA:
        	    drawVideoTextureDirectly(
           		    picLock->textureRect(),
           		    picLock->glTextures()[0],
           		    v_array );
                break;

            case PIX_FMT_YUV420P:
                Q_ASSERT( isYV12ToRgbShaderUsed() );
        	    drawYV12VideoTexture(
                    picLock,
                    picLock->textureRect(),
                    picLock->glTextures()[0],
                    picLock->glTextures()[1],
                    picLock->glTextures()[2],
                    v_array );
                break;

            case PIX_FMT_NV12:
                Q_ASSERT( isNV12ToRgbShaderUsed() );
        	    drawNV12VideoTexture(
                    picLock->textureRect(),
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
                //qDebug() << "new displayTime=" << QDateTime::fromMSecsSinceEpoch(m_lastDisplayedTime/1000).toString(QLatin1String("hh:mm:ss.zzz"));
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
	const QRectF& /*tex0Coords*/,
	unsigned int tex0ID,
	const float* v_array )
{
    cl_log.log( QString::fromAscii("QnGLRenderer::drawVideoTextureDirectly. texture %1").arg(tex0ID), cl_logDEBUG2 );

//    float tx_array[8] = {
//        0.0f, 0.0f,
//        tex0Coords.x(), 0.0f,
//        tex0Coords.x(), tex0Coords.y(),
//        0.0f, tex0Coords.y()
//    };
    float tx_array[8] = {
        0.0f, 0.0f,
        1, 0.0f,
        1, 1,
        0.0f, 1
//        (float)tex0->texCoords().x(), 0.0f,
//        (float)tex0->texCoords().x(), (float)tex0->texCoords().y(),
//        0.0f, (float)tex0->texCoords().y()
    };

    glEnable(GL_TEXTURE_2D);
    DEBUG_CODE(glCheckError("glEnable"));

    glBindTexture(GL_TEXTURE_2D, tex0ID);
    DEBUG_CODE(glCheckError("glBindTexture"));

    drawBindedTexture( v_array, tx_array );
}

void QnGLRenderer::drawYV12VideoTexture(
    const DecodedPictureToOpenGLUploader::ScopedPictureLock& /*picLock*/,
	const QRectF& tex0Coords,
	unsigned int tex0ID,
	unsigned int tex1ID,
	unsigned int tex2ID,
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

	m_yv12ToRgbShaderProgram->bind();
    m_yv12ToRgbShaderProgram->setParameters( m_brightness / 256.0f, m_contrast, m_hue, m_saturation, m_decodedPictureProvider.opacity() );

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

    m_yv12ToRgbShaderProgram->release();
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

	m_nv12ToRgbShaderProgram->bind();
	//m_nv12ToRgbShaderProgram->setParameters( m_brightness / 256.0f, m_contrast, m_hue, m_saturation, m_decodedPictureProvider.opacity() );
    m_nv12ToRgbShaderProgram->setYTexture( yPlaneTexID );
    m_nv12ToRgbShaderProgram->setUVTexture( uvPlaneTexID );
    m_nv12ToRgbShaderProgram->setOpacity( m_decodedPictureProvider.opacity() );
    m_nv12ToRgbShaderProgram->setColorTransform( QnNv12ToRgbShaderProgram::colorTransform(QnNv12ToRgbShaderProgram::YuvEbu) );

	glActiveTexture(GL_TEXTURE1);
	DEBUG_CODE(glCheckError("glActiveTexture"));
	glBindTexture(GL_TEXTURE_2D, yPlaneTexID);
	DEBUG_CODE(glCheckError("glBindTexture"));

	glActiveTexture(GL_TEXTURE0);
	DEBUG_CODE(glCheckError("glActiveTexture"));
	glBindTexture(GL_TEXTURE_2D, uvPlaneTexID);
	DEBUG_CODE(glCheckError("glBindTexture"));

    drawBindedTexture( v_array, tx_array );

    m_nv12ToRgbShaderProgram->release();
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

void QnGLRenderer::blockTimeValue(qint64  timestamp )
{
    QMutexLocker lock(&m_mutex);
    m_lastDisplayedTime = timestamp;
    m_timeChangeEnabled = false;
    //qDebug() << "blockTimeValue" << QDateTime::fromMSecsSinceEpoch(m_lastDisplayedTime/1000).toString(QLatin1String("hh:mm:ss.zzz"));;
}

void QnGLRenderer::unblockTimeValue()
{  
    QMutexLocker lock(&m_mutex);
    //qDebug() << "unblockTimeValue";
    m_timeChangeEnabled = true;
}

bool QnGLRenderer::isTimeBlocked() const
{
    return !m_timeChangeEnabled;
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
        && m_yv12ToRgbShaderProgram.get()
        && m_yv12ToRgbShaderProgram->isValid();
}

bool QnGLRenderer::isNV12ToRgbShaderUsed() const
{
    return (features() & QnGlFunctions::ArbPrograms)
        && (features() & QnGlFunctions::OpenGL1_3)
        && !(features() & QnGlFunctions::ShadersBroken)
        && !m_decodedPictureProvider.isForcedSoftYUV()
        && m_nv12ToRgbShaderProgram.get();
        //&& m_nv12ToRgbShaderProgram->isValid();
}
