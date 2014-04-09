/**********************************************************
* 24 oct 2012
* a.kolesnikov
***********************************************************/

#include "aggregationsurface.h"

#include <QtCore/QMutexLocker>

//#define GL_GLEXT_PROTOTYPES 1
//#ifdef Q_OS_MACX
//#include <glext.h>
//#else
//#include <GL/glext.h>
//#endif
//#define GL_GLEXT_PROTOTYPES 1
#include <QtGui/qopengl.h>

#include <utils/math/math.h>
#include <utils/common/log.h>
#include <utils/media/sse_helper.h>
#include <utils/color_space/yuvconvert.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include "opengl_renderer.h"

//#define PERFORMANCE_TEST

#ifdef PERFORMANCE_TEST
static const QSize surfaceSize = QSize( 640, 480 );
#else
static const QSize surfaceSize = QSize( 1920, 1080 );
#endif

static QString rectToString( const QRect& rect )
{
    return lit("(%1; %2; %3; %4)").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

namespace
{
    const int ROUND_COEFF = 8;

    int minPow2(int value)
    {
        int result = 1;
        while (value > result)
            result <<= 1;

        return result;
    }
} // anonymous namespace

static bool isYuvFormat( PixelFormat format )
{
    return format == PIX_FMT_YUV422P || format == PIX_FMT_YUV420P || format == PIX_FMT_YUV444P;
}

static int glRGBFormat( PixelFormat format )
{
    if( !isYuvFormat( format ) )
    {
        switch( format )
        {
            case PIX_FMT_RGBA:
                return GL_RGBA;
            case PIX_FMT_BGRA:
                return GL_BGRA_EXT;
            case PIX_FMT_RGB24:
                return GL_RGB;
            case PIX_FMT_BGR24:
                return GL_BGRA_EXT;
            default:
                break;
        }
    }
    return GL_RGBA;
}

//////////////////////////////////////////////////////////
// QnGlRendererTexture1
//////////////////////////////////////////////////////////
class QnGlRendererTexture1
{
public:
    QnGlRendererTexture1()
    : 
        m_allocated(false),
        m_internalFormat(-1),
        m_textureSize(QSize(0, 0)),
        m_contentSize(QSize(0, 0)),
        m_id(-1)
    {
    }

    ~QnGlRendererTexture1()
    {
        //NOTE we do not delete texture here because it belongs to auxiliary gl context which will be removed when these textures are not needed anymore
        if( m_id != (GLuint)-1 )
        {
            glDeleteTextures(1, &m_id);
            m_id = (GLuint)-1;
        }
    }

    const QVector2D &texCoords() const {
        return m_texCoords;
    }

    const QSize &textureSize() const {
        return m_textureSize;
    }

    const QSize &contentSize() const {
        return m_contentSize;
    }

    GLuint id() const {
        return m_id;
    }

    void ensureInitialized( int width, int height, int stride, int pixelSize, GLint internalFormat )
    {
        ensureAllocated();

        QSize contentSize = QSize(width, height);

        if( m_internalFormat == internalFormat && m_contentSize == contentSize )
            return;

        m_contentSize = contentSize;

        //QSize textureSize = QSize( qPower2Ceil((unsigned)stride / pixelSize, ROUND_COEFF), height );
        QSize textureSize = QSize( minPow2(stride / pixelSize) , minPow2(height) );
            

        if( m_textureSize.width() < textureSize.width() || m_textureSize.height() < textureSize.height() || m_internalFormat != internalFormat )
        {
            m_textureSize = textureSize;
            m_internalFormat = internalFormat;

            glBindTexture(GL_TEXTURE_2D, m_id);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, textureSize.width(), textureSize.height(), 0, internalFormat, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        else
        {
            textureSize = m_textureSize;
        }

        int roundedWidth = qPower2Ceil((unsigned) width, ROUND_COEFF);
        m_texCoords = QVector2D(
            static_cast<float>(roundedWidth) / textureSize.width(),
            static_cast<float>(height) / textureSize.height()
        );
    }

    void ensureAllocated()
    {
        if( m_allocated )
            return;
        glGenTextures( 1, &m_id );
        if( glGetError() == 0 )
            m_allocated = true;
    }

private:
    bool m_allocated;
    int m_internalFormat;
    QSize m_textureSize;
    QSize m_contentSize;
    QVector2D m_texCoords;
    GLuint m_id;
};


static QAtomicInt totalLockedRectCount = 0;

//////////////////////////////////////////////////////////
// AggregationSurface
//////////////////////////////////////////////////////////
AggregationSurface::AggregationSurface( PixelFormat format, const QSize& size )
:
    m_format( format ),
    m_textureFormat( PIX_FMT_NONE ),
    m_convertToRgb( false ),
    m_yuv2rgbBuffer( NULL ),
    m_yuv2rgbBufferLen( 0 ),
    m_fullRect( QPoint(0, 0), size ),
    m_lockedRectCount( 0 ),
    m_planeCount( 0 )
{
    for( uint i = 0; i < MAX_PLANE_COUNT; ++i )
    {
        m_textures[i].reset(new QnGlRendererTexture1());
        //m_textures[i]->ensureAllocated();
    }

    size_t pitch[3];
    memset( pitch, 0, sizeof(pitch) );
    size_t height[3];
    memset( height, 0, sizeof(height) );

    switch( m_format )
    {
        case PIX_FMT_YUV420P:
            //Y
            pitch[0] = qPower2Ceil( (unsigned int)m_fullRect.width(), ROUND_COEFF );
            height[0] = size.height();
            //U
            pitch[1] = pitch[0] / 2;
            height[1] = size.height() / 2;
            //V
            pitch[2] = pitch[0] / 2;
            height[2] = size.height() / 2;
            m_planeCount = 3;
            break;

        case PIX_FMT_NV12:
            //Y
            pitch[0] = qPower2Ceil( (unsigned int)m_fullRect.width(), ROUND_COEFF );
            height[0] = size.height();
            //UV
            pitch[1] = pitch[0];
            height[1] = size.height() / 2;
            m_planeCount = 2;
            break;

        case PIX_FMT_RGBA:
            //TODO/IMPL
            pitch[0] = qPower2Ceil( (unsigned int)m_fullRect.width(), ROUND_COEFF );
            height[0] = size.height();
            pitch[1] = pitch[0];
            height[1] = size.height() / 2;
            m_planeCount = 1;
            break;

        default:
            break;
    }

    m_buffers[0].pitch = pitch[0];
    m_buffers[0].buffer.resize( pitch[0]*height[0] );
    memset( const_cast<quint8*>(m_buffers[0].buffer.data()), 0, m_buffers[0].buffer.size() );

    if( pitch[1] )
    {
        m_buffers[1].pitch = pitch[1];
        m_buffers[1].buffer.resize( pitch[1]*height[1] );
        memset( const_cast<quint8*>(m_buffers[1].buffer.data()), 0, m_buffers[1].buffer.size() );
    }

    if( pitch[2] )
    {
        m_buffers[2].pitch = pitch[2];
        m_buffers[2].buffer.resize( pitch[2]*height[2] );
        std::fill( m_buffers[2].buffer.begin(), m_buffers[2].buffer.end(), 0 );
        memset( const_cast<quint8*>(m_buffers[2].buffer.data()), 0, m_buffers[2].buffer.size() );
    }
}

void AggregationSurface::ensureUploadedToOGL( const QRect& rect, qreal opacity )
{
    QMutexLocker uploadLock( &m_uploadMutex );  //TODO/IMPL avoid this lock

    //TODO/IMPL it makes sense to load bounding rect of m_lockedSysMemBufferRegion for optimization
    const QRegion regionBeingLoaded = m_fullRect;
    QRegion lockedRegionBeingLoaded;
    size_t lockedRectCount = 0;
    {
        QMutexLocker lk( &m_mutex );
        if( m_glMemRegion.contains( rect ) )
        {
            NX_LOG( lit("AggregationSurface(%1)::ensureUploadedToOGL. Requested region %2 is uploaded already. Total locked rects count %3, bounding rect %4").
                arg((size_t)this, 0, 16).arg(rectToString(rect)).arg(lockedRectCount).arg(rectToString(lockedRegionBeingLoaded.boundingRect())), cl_logDEBUG1 );
            return; //region already uploaded
        }
        m_invalidatedRegion = QRegion();
        m_glMemRegion = QRegion();
        lockedRectCount = m_lockedRectCount;
        lockedRegionBeingLoaded = m_lockedSysMemBufferRegion;
    }

    NX_LOG( lit("AggregationSurface(%1)::ensureUploadedToOGL. Uploading aggregation surface containing %2 locked rects (bounding rect %3) to opengl...").
        arg((size_t)this, 0, 16).arg(lockedRectCount).arg(rectToString(lockedRegionBeingLoaded.boundingRect())), cl_logDEBUG1 );

    unsigned int r_w[3] = { (uint)m_fullRect.width(), (uint)m_fullRect.width() / 2, (uint)m_fullRect.width() / 2 }; // real_width / visible
    unsigned int h[3] = { (uint)m_fullRect.height(), (uint)m_fullRect.height() / 2, (uint)m_fullRect.height() / 2 };

    switch( m_format )
    {
        case PIX_FMT_YUV444P:
            r_w[1] = r_w[2] = m_fullRect.width();
        // fall through
        case PIX_FMT_YUV422P:
            h[1] = h[2] = m_fullRect.height();
            break;
        default:
            break;
    }

    if( (m_format == PIX_FMT_YUV420P || m_format == PIX_FMT_YUV422P || m_format == PIX_FMT_YUV444P) && !m_convertToRgb )
    {
        //using pixel shader for yuv->rgb conversion
        for( int i = 0; i < 3; ++i )
        {
            QnGlRendererTexture1* texture = m_textures[i].data();
            texture->ensureInitialized( r_w[i], h[i], m_buffers[i].pitch, 1, GL_LUMINANCE );

#ifdef USE_PBO
            ensurePBOInitialized( emptyPictureBuf, lineSizes[i]*h[i] );
            d->glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, emptyPictureBuf->pboID() );
            //GLvoid* pboData = d->glMapBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB );
            //Q_ASSERT( pboData );
            ////memcpy( pboData, planes[i], lineSizes[i]*h[i] );
            //memcpy_sse4_stream_stream( (__m128i*)pboData, (__m128i*)planes[i], lineSizes[i]*h[i] );
            //d->glUnmapBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB );
            d->glBufferSubDataARB(
                GL_PIXEL_UNPACK_BUFFER_ARB,
                0,
                lineSizes[i]*h[i],
                planes[i] );
#endif

            glBindTexture( GL_TEXTURE_2D, texture->id() );

            GLfloat w0 = 0;
//            glGetTexLevelParameterfv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w0 );
            GLfloat h0 = 0;
//            glGetTexLevelParameterfv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h0 );

//            glPixelStorei( GL_UNPACK_ROW_LENGTH, m_buffers[i].pitch );
            Q_ASSERT( m_buffers[i].pitch >= qPower2Ceil(r_w[i],ROUND_COEFF) );

            #ifndef USE_PBO
                loadImageData(texture->textureSize().width(),texture->textureSize().height(),m_buffers[i].pitch,h[i],1,GL_LUMINANCE,m_buffers[i].buffer.data());
            #else
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,qPower2Ceil(r_w[i],ROUND_COEFF),h[i],GL_LUMINANCE, GL_UNSIGNED_BYTE,NULL);
            #endif
            /*
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            qPower2Ceil(r_w[i],ROUND_COEFF),
                            h[i],
                            GL_LUMINANCE, GL_UNSIGNED_BYTE,
#ifndef USE_PBO
                            m_buffers[i].buffer.data()
#else
                            NULL
#endif
                            );*/

#ifdef USE_PBO
            d->glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );
#endif

            glCheckError("glTexSubImage2D");
//            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glCheckError("glPixelStorei");
        }
        m_textureFormat = PIX_FMT_YUV420P;
    }
    else if( m_format == PIX_FMT_NV12 && !m_convertToRgb )
    {
        for( int i = 0; i < 2; ++i )
        {
            QnGlRendererTexture1* texture = m_textures[i].data();
            if( i == 0 )    //Y-plane
                texture->ensureInitialized( m_fullRect.width(), m_fullRect.height(), m_buffers[i].pitch, 1, GL_LUMINANCE );
            else
                texture->ensureInitialized( m_fullRect.width() / 2, m_fullRect.height() / 2, m_buffers[i].pitch, 2, GL_LUMINANCE_ALPHA );
            glBindTexture( GL_TEXTURE_2D, texture->id() );
//            glPixelStorei( GL_UNPACK_ROW_LENGTH, i == 0 ? m_buffers[0].pitch : (m_buffers[1].pitch/2) );

            loadImageData(  texture->textureSize().width(),
                            texture->textureSize().height(),
                            i == 0 ? m_buffers[0].pitch : (m_buffers[1].pitch/2),
                            i == 0 ? m_fullRect.height() : m_fullRect.height() / 2,
                            i == 0 ? 1 : 2,
                            i == 0 ? GL_LUMINANCE : GL_LUMINANCE_ALPHA,
                            m_buffers[i].buffer.data() );
            /*
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            i == 0 ? qPower2Ceil((unsigned int)m_fullRect.width(),ROUND_COEFF) : m_fullRect.width() / 2,
                            i == 0 ? m_fullRect.height() : m_fullRect.height() / 2,
                            i == 0 ? GL_LUMINANCE : GL_LUMINANCE_ALPHA,
                            GL_UNSIGNED_BYTE, m_buffers[i].buffer.data() );*/
            glCheckError("glTexSubImage2D");
            //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            //glCheckError("glPixelStorei");
        }
        m_textureFormat = PIX_FMT_NV12;
    }
    else
    {
        //software conversion data to rgb
        QnGlRendererTexture1* texture = m_textures[0].data();

        int bytesPerPixel = 1;
        if( !isYuvFormat(m_format) )
        {
            if( m_format == PIX_FMT_RGB24 || m_format == PIX_FMT_BGR24 )
                bytesPerPixel = 3;
            else
                bytesPerPixel = 4;
        }

        texture->ensureInitialized( r_w[0], h[0], m_buffers[0].pitch, bytesPerPixel, GL_RGBA );
        glBindTexture(GL_TEXTURE_2D, texture->id());

        uchar* pixels = const_cast<quint8*>(m_buffers[0].buffer.data());
        if( isYuvFormat(m_format) )
        {
            int size = 4 * m_buffers[0].pitch * h[0];
            if (m_yuv2rgbBufferLen < size)
            {
                m_yuv2rgbBufferLen = size;
                qFreeAligned(m_yuv2rgbBuffer);
                m_yuv2rgbBuffer = (uchar*)qMallocAligned(size, CL_MEDIA_ALIGNMENT);
            }
            pixels = m_yuv2rgbBuffer;
        }

        int lineInPixelsSize = m_buffers[0].pitch;
        switch (m_format)
        {
            case PIX_FMT_YUV420P:
                if (useSSE2())
                {
                    yuv420_argb32_simd_intr(pixels, m_buffers[0].buffer.data(), m_buffers[2].buffer.data(), m_buffers[1].buffer.data(),
                        qPower2Ceil(r_w[0],ROUND_COEFF),
                        h[0],
                        4 * m_buffers[0].pitch,
                        m_buffers[0].pitch, m_buffers[1].pitch, opacity*255);
                }
                else {
                    NX_LOG("CPU does not contain SSE2 module. Color space convert is not implemented", cl_logWARNING);
                }
                break;

            case PIX_FMT_YUV422P:
                if (useSSE2())
                {
                    yuv422_argb32_simd_intr(pixels, m_buffers[0].buffer.data(), m_buffers[2].buffer.data(), m_buffers[1].buffer.data(),
                        qPower2Ceil(r_w[0],ROUND_COEFF),
                        h[0],
                        4 * m_buffers[0].pitch,
                        m_buffers[0].pitch, m_buffers[1].pitch, opacity*255);
                }
                else {
                    NX_LOG("CPU does not contain SSE2 module. Color space convert is not implemented", cl_logWARNING);
                }
                break;

            case PIX_FMT_YUV444P:
                if (useSSE2())
                {
                    yuv444_argb32_simd_intr(pixels, m_buffers[0].buffer.data(), m_buffers[2].buffer.data(), m_buffers[1].buffer.data(),
                        qPower2Ceil(r_w[0],ROUND_COEFF),
                        h[0],
                        4 * m_buffers[0].pitch,
                        m_buffers[0].pitch, m_buffers[1].pitch, opacity*255);
                }
                else {
                    NX_LOG("CPU does not contain SSE2 module. Color space convert is not implemented", cl_logWARNING);
                }
                break;

            case PIX_FMT_RGB24:
            case PIX_FMT_BGR24:
                lineInPixelsSize /= 3;
                break;

            default:
                lineInPixelsSize /= 4; // RGBA, BGRA
                break;
        }

//        glPixelStorei(GL_UNPACK_ROW_LENGTH, lineInPixelsSize);
        glCheckError("glPixelStorei");
        
        loadImageData(texture->textureSize().width(),texture->textureSize().height(),lineInPixelsSize,h[0],bytesPerPixel,glRGBFormat(m_format),pixels);
        /*
        glTexSubImage2D(GL_TEXTURE_2D, 0,
            0, 0,
            qPower2Ceil(r_w[0],ROUND_COEFF),
            h[0],
            glRGBFormat(m_format), GL_UNSIGNED_BYTE, pixels);*/
        glCheckError("glTexSubImage2D");

//        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glCheckError("glPixelStorei");

        m_textureFormat = PIX_FMT_RGBA;

        // TODO: #Elric free memory immediately for still images
    }

    {
        QMutexLocker lk( &m_mutex );
        //while we were loading some regions could be invalidated and/or locked
        m_glMemRegion = regionBeingLoaded - m_invalidatedRegion;
    }
}

const std::vector<GLuint>& AggregationSurface::glTextures() const
{
    m_glTextureIDs.resize( MAX_PLANE_COUNT );
    for( size_t i = 0; i < MAX_PLANE_COUNT; ++i )
        m_glTextureIDs[i] = m_textures[i]->id();
    return m_glTextureIDs;
}

void AggregationSurface::uploadData( const QRect& destRect, uint8_t* planes[], int lineSizes[] )
{
    Q_ASSERT( destRect.bottomRight().x() < m_fullRect.width() );
    Q_ASSERT( destRect.bottomRight().y() < m_fullRect.height() );

    QMutexLocker uploadLock( &m_uploadMutex );

#ifndef PERFORMANCE_TEST
    {
        QMutexLocker lk( &m_mutex );
        m_glMemRegion -= destRect;
        m_invalidatedRegion += destRect;
    }

    for( size_t i = 0; i < m_planeCount; ++i )
    {
        int horizontalResolution = i == 0 ? 1 : (m_format == PIX_FMT_YUV420P ? 2 : 1);
        int verticalResolution = i == 0 ? 1 : 2;

        const int startLine = destRect.y() / verticalResolution;
        const int endLine = (destRect.bottomRight().y()+1) / verticalResolution;
        const int dstLineOffset = destRect.x() / horizontalResolution;
        const int bytesToCopy = destRect.width() / horizontalResolution;
        for( int y = startLine; y < endLine; ++y )
        {
            memcpy(
                const_cast<quint8*>(m_buffers[i].buffer.data()) + y*m_buffers[i].pitch + dstLineOffset,
                planes[i] + (y-startLine)*lineSizes[i],
                bytesToCopy );
        }
    }
#endif
}

bool AggregationSurface::lockRect( const QRect& rect )
{
    QMutexLocker lk( &m_mutex );

    //checking that rect is not locked
    const QRegion& invertedRegion = m_lockedSysMemBufferRegion.xored( QRect( QPoint(0, 0), surfaceSize ) );
    if( !invertedRegion.contains( rect ) )
        return false;   //rect is already locked
    m_lockedSysMemBufferRegion += rect;

    totalLockedRectCount.ref();

    NX_LOG( lit("AggregationSurface::lockRect. Locked rect %1. Total locked bounding rect %2, total locked rects %3").
        arg(rectToString(rect)).arg(rectToString(m_lockedSysMemBufferRegion.boundingRect())).arg(totalLockedRectCount.load()), cl_logDEBUG1 );

    ++m_lockedRectCount;

    return true;
}

QRect AggregationSurface::findAndLockRect( const QSize& requestedRectSize )
{
#ifdef PERFORMANCE_TEST
    return QRect( QPoint(0, 0), requestedRectSize );
#else
    QMutexLocker lk( &m_mutex );

    QRegion invertedRegion;
    //for( int i = 0; i < 1000; ++i )
        invertedRegion = m_lockedSysMemBufferRegion.xored( QRect( QPoint(0, 0), surfaceSize ) );
    //checking, if rect of size requestedRectSize can be placed on invertedRegion
    const QVector<QRect>& nonOverlappingUnlockedRects = invertedRegion.rects();
    for( QVector<QRect>::size_type i = 0; i < nonOverlappingUnlockedRects.size(); ++i )
    {
        if( (nonOverlappingUnlockedRects[i].size().width() >= requestedRectSize.width()) &&
            (nonOverlappingUnlockedRects[i].size().height() >= requestedRectSize.height()) )     //nonOverlappingUnlockedRects[i].size() > requestedRectSize
        {
            //locking rect and returning
            //QRect unusedRect( nonOverlappingUnlockedRects[i].topLeft(), requestedRectSize );
            QRect unusedRect(
                nonOverlappingUnlockedRects[i].bottomRight()-QPoint(requestedRectSize.width(), requestedRectSize.height())+QPoint(1,1),
                nonOverlappingUnlockedRects[i].bottomRight() );
            m_lockedSysMemBufferRegion += unusedRect;

            totalLockedRectCount.ref();

            NX_LOG( lit("AggregationSurface::findAndLockRect. Locked rect %1 of size %2x%3. Total locked bounding rect %4, total locked rects %5").
                arg(rectToString(unusedRect)).arg(requestedRectSize.width()).arg(requestedRectSize.height()).
                arg(rectToString(m_lockedSysMemBufferRegion.boundingRect())).arg(totalLockedRectCount.load()), cl_logDEBUG1 );

            ++m_lockedRectCount;

            return unusedRect;
        }
    }

    //TODO/BUG this implementation is buggy. E.g., if nonOverlappingUnlockedRects is (0,0,5,10) and (5,5,10,10) and requested size is (10, 5) 
        //we won't lock anything, although rect (0,5,10,10) is available. Rect coords in this example are (x1,y1,x2,y2)

    return QRect();
#endif
}

void AggregationSurface::unlockRect( const QRect& rect )
{
#ifdef PERFORMANCE_TEST
#else
    QMutexLocker lk( &m_mutex );
    m_lockedSysMemBufferRegion -= rect;
    totalLockedRectCount.deref();
    NX_LOG( lit("AggregationSurface::unlockRect. Unlocked rect %1. Total locked bounding rect %2, total locked rects %3").
        arg(rectToString(rect)).arg(rectToString(m_lockedSysMemBufferRegion.boundingRect())).arg(totalLockedRectCount.load()), cl_logDEBUG1 );

    --m_lockedRectCount;
#endif
}

QRectF AggregationSurface::translateRectToTextureRect( const QRect& rect ) const
{
    return QRectF(
        (float)rect.x() / m_fullRect.width(),
        (float)rect.y() / m_fullRect.height(),
        (float)rect.width() / m_fullRect.width(),
        (float)rect.height() / m_fullRect.height() );
}


//////////////////////////////////////////////////////////
// AggregationSurfaceRect
//////////////////////////////////////////////////////////
AggregationSurfaceRect::AggregationSurfaceRect( const QSharedPointer<AggregationSurface>& surface, const QRect& rect )
:
    m_surface( surface ),
    m_rect( rect )
{
    if( m_surface )
        m_texCoords = m_surface->translateRectToTextureRect( m_rect );
}

AggregationSurfaceRect::~AggregationSurfaceRect()
{
    m_surface->unlockRect( m_rect );
}

const QSharedPointer<AggregationSurface>& AggregationSurfaceRect::surface() const
{
    return m_surface;
}

const QRect& AggregationSurfaceRect::rect() const
{
    return m_rect;
}

const QRectF& AggregationSurfaceRect::texCoords() const
{
    return m_texCoords;
}

void AggregationSurfaceRect::uploadData(
    uint8_t* planes[],
    int lineSizes[],
    size_t /*planeCount*/ )
{
    m_surface->uploadData( m_rect, planes, lineSizes );
}

void AggregationSurfaceRect::ensureUploadedToOGL( qreal opacity )
{
    m_surface->ensureUploadedToOGL( m_rect, opacity );
}


//////////////////////////////////////////////////////////
// AggregationSurfacePool
//////////////////////////////////////////////////////////
Q_GLOBAL_STATIC( AggregationSurfacePool, aggregationSurfacePoolInstanceGetter );

QSharedPointer<AggregationSurfaceRect> AggregationSurfacePool::takeSurfaceRect(
    const GLContext* glContext,
    const PixelFormat format,
    const QSize& requiredEmptySize )
{
    if( requiredEmptySize.isEmpty() || requiredEmptySize.isNull() || !requiredEmptySize.isValid() )
        return QSharedPointer<AggregationSurfaceRect>();

    QMutexLocker lk( &m_mutex );

    std::pair<AggregationSurfaceContainer::const_iterator, AggregationSurfaceContainer::const_iterator>
        surfacesRange = m_surfaces.equal_range( std::make_pair( glContext, format ) );

    for( AggregationSurfaceContainer::const_iterator
        it = surfacesRange.first;
        it != surfacesRange.second;
        ++it )
    {
        const QRect& lockedRect = it->second->findAndLockRect( requiredEmptySize );
        if( lockedRect.isNull() )
            continue;

        return QSharedPointer<AggregationSurfaceRect>( new AggregationSurfaceRect( it->second, lockedRect ) );
    }

    NX_LOG( lit("AggregationSurfacePool::takeSurfaceRect. Creating new surface"), cl_logDEBUG1 );

    //creating new surface
    QSharedPointer<AggregationSurface> newSurface( new AggregationSurface( format, surfaceSize ) );
    m_surfaces.insert( std::make_pair( std::make_pair( glContext, format ), newSurface ) );
    const QRect& lockedRect = newSurface->findAndLockRect( requiredEmptySize );
    Q_ASSERT( !lockedRect.isNull() );

    return QSharedPointer<AggregationSurfaceRect>( new AggregationSurfaceRect( newSurface, lockedRect ) );
}

void AggregationSurfacePool::removeSurfaces( GLContext* const /*glContext*/ )
{
    QMutexLocker lk( &m_mutex );
    //TODO/IMPL
}

AggregationSurfacePool* AggregationSurfacePool::instance()
{
    return aggregationSurfacePoolInstanceGetter();
}
