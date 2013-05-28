/**********************************************************
* 08 oct 2012
* a.kolesnikov
***********************************************************/

#include "decodedpicturetoopengluploader.h"

#include <algorithm>

#include <QMutexLocker>

#ifdef _WIN32
#include <D3D9.h>
#include <DXerr.h>
#endif
#define GL_GLEXT_PROTOTYPES 1
#include <GL/glext.h>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include <utils/color_space/yuvconvert.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/graphics/opengl/gl_context_data.h>

#include "decodedpicturetoopengluploadercontextpool.h"

//#define RENDERER_SUPPORTS_NV12
#ifdef _WIN32
//#define USE_PBO
#endif

#ifdef USE_PBO
#include <smmintrin.h>
//#define DIRECT_COPY
#ifdef DIRECT_COPY
//#define USE_SINGLE_PBO_PER_FRAME
#endif
//#define USE_SINGLE_PBO_PER_FRAME
#endif

//#define UPLOAD_SYSMEM_FRAMES_ASYNC
#define UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
//#define USE_MIN_GL_TEXTURES

#if defined(UPLOAD_SYSMEM_FRAMES_ASYNC) && defined(UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD)
#error "UPLOAD_SYSMEM_FRAMES_ASYNC and UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD cannot be defined simultaneously"
#endif

#define ASYNC_UPLOADING_USED
#if defined(UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD) && !defined(UPLOAD_SYSMEM_FRAMES_ASYNC) && !defined(_WIN32)
#undef ASYNC_UPLOADING_USED
#endif

#define PERFORMANCE_TEST
#ifdef PERFORMANCE_TEST
//#define DISABLE_FRAME_DOWNLOAD
//#define DISABLE_FRAME_UPLOAD
//#define PARTIAL_FRAME_UPLOAD
//#define SINGLE_STREAM_UPLOAD
#endif

//#define SYNC_UPLOADING_WITH_GLFENCE

//preceding bunch of macro will be removed after this functionality has been tested and works as expected


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

#ifdef _WIN32
#elif __APPLE__
unsigned int GetTickCount()
{
    return QDateTime::currentMSecsSinceEpoch();
}
#else
unsigned int GetTickCount()
{
    struct timespec tp;
    memset( &tp, 0, sizeof(tp) );
    clock_gettime( CLOCK_MONOTONIC, &tp );
    return tp.tv_sec*1000 + tp.tv_nsec / 1000000;
}
#endif

class BitrateCalculator
{
public:
    BitrateCalculator()
    :
        m_startCalcTick( 0 ),
        m_bytes( 0 )
    {
    }

    void bytesProcessed( unsigned int byteCount )
    {
        m_mtx.lock();

        const unsigned int currentTick = GetTickCount();
        if( currentTick - m_startCalcTick > 5000 )
        {
            NX_LOG( QString::fromAscii("In previous %1 ms to video mem moved %2 Mb. Transfer rate %3 Mb/second").
                arg(currentTick - m_startCalcTick).arg(m_bytes/1000000.0).arg(m_bytes /1000.0 / (currentTick - m_startCalcTick)), cl_logDEBUG1 );
            m_startCalcTick = currentTick;
            m_bytes = 0;
        }
        m_bytes += byteCount;

        m_mtx.unlock();
    }

private:
    unsigned int m_startCalcTick;
    quint64 m_bytes;
    QMutex m_mtx;
};

static BitrateCalculator bitrateCalculator;


// -------------------------------------------------------------------------- //
// DecodedPictureToOpenGLUploaderPrivate
// -------------------------------------------------------------------------- //
class DecodedPictureToOpenGLUploaderPrivate
:
    public QnGlFunctions
{
    Q_DECLARE_TR_FUNCTIONS(DecodedPictureToOpenGLUploaderPrivate);

public:
    DecodedPictureToOpenGLUploaderPrivate(const QGLContext *context):
        QnGlFunctions(context),
        supportsNonPower2Textures(false)
    {
        QByteArray extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        QByteArray version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        QByteArray renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

        /* Maximal texture size. */
        int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
        NX_LOG(QString(QLatin1String("OpenGL max texture size: %1.")).arg(maxTextureSize), cl_logINFO);

        /* Clamp constant. */
        clampConstant = GL_CLAMP;
        if (extensions.contains("GL_EXT_texture_edge_clamp") || extensions.contains("GL_SGIS_texture_edge_clamp") || version >= QByteArray("1.2.0"))
            clampConstant = GL_CLAMP_TO_EDGE;

        /* Check for non-power of 2 textures. */
        supportsNonPower2Textures = extensions.contains("GL_ARB_texture_non_power_of_two");
    }

    ~DecodedPictureToOpenGLUploaderPrivate()
    {
        for( int i = 0; i < FILLER_COUNT; ++i )
        {
            if( m_fillers[i].size > 0 )
            {
                qFreeAligned( m_fillers[i].data );
                m_fillers[i].data = NULL;
                m_fillers[i].size = 0;
            }
        }
    }

    unsigned char* filler( unsigned char value, size_t size )
    {
        QMutexLocker lk( &m_fillerMutex );

        Filler& filler = m_fillers[value];
        if( filler.size < size )
        {
            filler.size = size;
            if( filler.data )
                qFreeAligned( filler.data );
            filler.data = (unsigned char*)qMallocAligned( filler.size, FILLER_BUF_ALIGNMENT );
            memset( filler.data, value, size );
        }

        return filler.data;
    }

public:
    GLint clampConstant;
    bool supportsNonPower2Textures;
private:
    struct Filler
    {
        unsigned char* data;
        size_t size;

        Filler()
        :
            data( NULL ),
            size( 0 )
        {
        }
    };

    static const int FILLER_COUNT = 256;
    static const int FILLER_BUF_ALIGNMENT = 32;

    QMutex m_fillerMutex;
    Filler m_fillers[FILLER_COUNT];
};

// -------------------------------------------------------------------------- //
// QnGlRendererTexture
// -------------------------------------------------------------------------- //
class QnGlRendererTexture {
public:
    QnGlRendererTexture( const QSharedPointer<DecodedPictureToOpenGLUploaderPrivate>& renderer )
    : 
        m_allocated(false),
        m_internalFormat(-1),
        m_textureSize(QSize(0, 0)),
        m_contentSize(QSize(0, 0)),
        m_id(std::numeric_limits<GLuint>::max()),
        m_fillValue(-1),
        m_renderer(renderer)
    {}

    ~QnGlRendererTexture() {
        //NOTE we do not delete texture here because it belongs to auxiliary gl context which will be removed when these textures are not needed anymore
        if( m_id != std::numeric_limits<GLuint>::max() )
        {
            glDeleteTextures(1, &m_id);
            m_id = std::numeric_limits<GLuint>::max();
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

    void ensureInitialized(int width, int height, int stride, int pixelSize, GLint internalFormat, int internalFormatPixelSize, int fillValue) {
        Q_ASSERT(m_renderer.data() != NULL);

        ensureAllocated();

        QSize contentSize = QSize(width, height);

        if(m_internalFormat == internalFormat && m_fillValue == fillValue && m_contentSize == contentSize)
            return;

        m_contentSize = contentSize;

        QSize textureSize = QSize(
            m_renderer->supportsNonPower2Textures ? qPower2Ceil((unsigned)stride / pixelSize, ROUND_COEFF) : minPow2(stride / pixelSize),
            m_renderer->supportsNonPower2Textures ? height                                   : minPow2(height)
        );

        if(m_textureSize.width() < textureSize.width() || m_textureSize.height() < textureSize.height() || m_internalFormat != internalFormat) {
            m_textureSize = textureSize;
            m_internalFormat = internalFormat;

            glBindTexture(GL_TEXTURE_2D, m_id);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, textureSize.width(), textureSize.height(), 0, internalFormat, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_renderer->clampConstant);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_renderer->clampConstant);
        } else {
            textureSize = m_textureSize;
        }

        int roundedWidth = qPower2Ceil((unsigned) width, ROUND_COEFF);
        m_texCoords = QVector2D(
            static_cast<float>(roundedWidth) / textureSize.width(),
            static_cast<float>(height) / textureSize.height()
        );

        if(fillValue != -1) {
            m_fillValue = fillValue;

            /* To prevent uninitialized pixels on the borders of the image from
             * leaking into the rendered quad due to linear filtering, 
             * we fill them with black. 
             * 
             * Note that this also must be done when contents size changes because
             * in this case even though the border pixels are initialized, they are
             * initialized with old contents, which is probably not what we want. */
            size_t fillSize = qMax(textureSize.height(), textureSize.width()) * ROUND_COEFF * internalFormatPixelSize * 16;
            uchar *filler = m_renderer->filler(fillValue, fillSize);

            if (roundedWidth < textureSize.width()) {
                glBindTexture(GL_TEXTURE_2D, m_id);

                GLint textureWidth = 0;
                glGetTexLevelParameteriv( GL_TEXTURE_2D, 0,  GL_TEXTURE_WIDTH, &textureWidth );
                GLint textureHeight = 0;
                glGetTexLevelParameteriv( GL_TEXTURE_2D, 0,  GL_TEXTURE_HEIGHT, &textureHeight );
                //Q_ASSERT( textureSize == QSize(textureWidth, textureHeight) );

                glTexSubImage2D(
                    GL_TEXTURE_2D, 
                    0,
                    roundedWidth,
                    0,
                    qMin(ROUND_COEFF, textureSize.width() - roundedWidth),
                    textureSize.height(),
                    internalFormat, 
                    GL_UNSIGNED_BYTE, 
                    filler );
                bitrateCalculator.bytesProcessed( qMin(ROUND_COEFF, textureSize.width() - roundedWidth)*textureSize.height()*internalFormatPixelSize );
                DEBUG_CODE(glCheckError("glTexSubImage2D"));
            }

            if (height < textureSize.height()) {
                glBindTexture(GL_TEXTURE_2D, m_id);
                glTexSubImage2D(
                    GL_TEXTURE_2D, 
                    0,
                    0, 
                    height,
                    textureSize.width(),
                    qMin(ROUND_COEFF, textureSize.height() - height),
                    internalFormat, 
                    GL_UNSIGNED_BYTE, 
                    filler );
                bitrateCalculator.bytesProcessed( textureSize.width()*qMin(ROUND_COEFF, textureSize.height() - height)*4 );
                DEBUG_CODE(glCheckError("glTexSubImage2D"));
            }
        }
    }

    void ensureAllocated() {
        if(m_allocated)
            return;

        glGenTextures(1, &m_id);
        DEBUG_CODE(glCheckError("glGenTextures"));

        m_allocated = true;
    }

private:
    bool m_allocated;
    int m_internalFormat;
    QSize m_textureSize;
    QSize m_contentSize;
    QVector2D m_texCoords;
    GLubyte m_fillColor;
    GLuint m_id;
    int m_fillValue;
    QSharedPointer<DecodedPictureToOpenGLUploaderPrivate> m_renderer;
};


//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploader::UploadedPicture
//////////////////////////////////////////////////////////
DecodedPictureToOpenGLUploader::UploadedPicture::PBOData::PBOData()
:
    id( std::numeric_limits<GLuint>::max() ),
    sizeBytes( 0 )
{
}


PixelFormat DecodedPictureToOpenGLUploader::UploadedPicture::colorFormat() const
{
    return m_colorFormat;
}

void DecodedPictureToOpenGLUploader::UploadedPicture::setColorFormat( PixelFormat newFormat )
{
    m_colorFormat = newFormat;
}

int DecodedPictureToOpenGLUploader::UploadedPicture::width() const
{
    return m_width;
}

int DecodedPictureToOpenGLUploader::UploadedPicture::height() const
{
    return m_height;
}

//const QVector2D& DecodedPictureToOpenGLUploader::UploadedPicture::texCoords() const
//{
//    //TODO/IMPL
//    return m_textures[0]->texCoords();
//}

QRectF DecodedPictureToOpenGLUploader::UploadedPicture::textureRect() const
{
#ifdef GL_COPY_AGGREGATION
    Q_ASSERT( m_surfaceRect && m_surfaceRect->surface() );
    return m_surfaceRect->texCoords();
#else
    const QVector2D& v = m_textures[0]->texCoords();
    return QRectF( 0, 0, v.x(), v.y() );
#endif
}

const std::vector<GLuint>& DecodedPictureToOpenGLUploader::UploadedPicture::glTextures() const
{
#ifdef GL_COPY_AGGREGATION
    Q_ASSERT( m_surfaceRect && m_surfaceRect->surface() );
    return m_surfaceRect->surface()->glTextures();
#else
    //TODO/IMPL
    m_picTextures.resize( MAX_TEXTURE_COUNT );
    for( size_t i = 0; i < MAX_TEXTURE_COUNT; ++i )
        m_picTextures[i] = m_textures[i]->id();
    return m_picTextures;
#endif
}

unsigned int DecodedPictureToOpenGLUploader::UploadedPicture::sequence() const
{
    return m_sequence;
}

quint64 DecodedPictureToOpenGLUploader::UploadedPicture::pts() const
{
    return m_pts;
}

QnMetaDataV1Ptr DecodedPictureToOpenGLUploader::UploadedPicture::metadata() const
{
    return m_metadata;
}

QnGlRendererTexture* DecodedPictureToOpenGLUploader::UploadedPicture::texture( int index ) const
{
    return m_textures[index].data();
}

GLuint DecodedPictureToOpenGLUploader::UploadedPicture::pboID( int index ) const
{
    return m_pbo[index].id;
}

int DecodedPictureToOpenGLUploader::UploadedPicture::flags() const
{
    return m_flags;
}

#ifdef GL_COPY_AGGREGATION
void DecodedPictureToOpenGLUploader::UploadedPicture::setAggregationSurfaceRect( const QSharedPointer<AggregationSurfaceRect>& surfaceRect )
{
    m_surfaceRect = surfaceRect;
}

const QSharedPointer<AggregationSurfaceRect>& DecodedPictureToOpenGLUploader::UploadedPicture::aggregationSurfaceRect() const
{
    return m_surfaceRect;
}
#endif


//const int DecodedPictureToOpenGLUploader::UploadedPicture::MAX_TEXTURE_COUNT;

DecodedPictureToOpenGLUploader::UploadedPicture::UploadedPicture( DecodedPictureToOpenGLUploader* const uploader )
:
    m_colorFormat( PIX_FMT_NONE ),
    m_width( 0 ),
    m_height( 0 ),
    m_sequence( 0 ),
    m_pts( 0 ),
    m_skippingForbidden( false ),
    m_flags( 0 ),
    m_glFence( uploader->d.data() )
{
    //TODO/IMPL allocate textures when needed, because not every format require 3 planes
    for( size_t i = 0; i < MAX_TEXTURE_COUNT; ++i )
        m_textures[i].reset( new QnGlRendererTexture(uploader->d) );
}

DecodedPictureToOpenGLUploader::UploadedPicture::~UploadedPicture()
{
    //TODO/IMPL
}


//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploader::ScopedPictureLock
//////////////////////////////////////////////////////////
DecodedPictureToOpenGLUploader::ScopedPictureLock::ScopedPictureLock( const DecodedPictureToOpenGLUploader& uploader )
:
    m_uploader( uploader ),
    m_picture( uploader.getUploadedPicture() )
{
}

DecodedPictureToOpenGLUploader::ScopedPictureLock::~ScopedPictureLock()
{
    if( m_picture )
    {
        m_uploader.pictureDrawingFinished( m_picture );
        m_picture = NULL;
    }
}

DecodedPictureToOpenGLUploader::UploadedPicture* DecodedPictureToOpenGLUploader::ScopedPictureLock::operator->()
{
    return m_picture;
}

const DecodedPictureToOpenGLUploader::UploadedPicture* DecodedPictureToOpenGLUploader::ScopedPictureLock::operator->() const
{
    return m_picture;
}

DecodedPictureToOpenGLUploader::UploadedPicture* DecodedPictureToOpenGLUploader::ScopedPictureLock::get()
{
    return m_picture;
}

const DecodedPictureToOpenGLUploader::UploadedPicture* DecodedPictureToOpenGLUploader::ScopedPictureLock::get() const
{
    return m_picture;
}


//////////////////////////////////////////////////////////
// AsyncPicDataUploader
//////////////////////////////////////////////////////////

#ifdef _WIN32
inline void memcpy_stream_load( __m128i* dst, __m128i* src, size_t sz )
{
    const __m128i* const src_end = src + sz / sizeof(__m128i);
    while( src < src_end )
    {
         __m128i x1 = _mm_stream_load_si128( src );
         __m128i x2 = _mm_stream_load_si128( src+1 );
         __m128i x3 = _mm_stream_load_si128( src+2 );
         __m128i x4 = _mm_stream_load_si128( src+3 );

         src += 4;

         _mm_store_si128( dst, x1 );
         _mm_store_si128( dst+1, x2 );
         _mm_store_si128( dst+2, x3 );
         _mm_store_si128( dst+3, x4 );

         dst += 4;
    }
}

/*!
    \param nv12UVPlaneSize expression (\a nv12UVPlaneSize % 64) MUST be 0!
*/
inline void streamLoadAndDeinterleaveNV12UVPlane(
    __m128i* nv12UVPlane,
    size_t nv12UVPlaneSize,
    __m128i* yv12UPlane,
    __m128i* yv12VPlane )
{
    //Q_ASSERT( nv12UVPlaneSize % 64 == 0 );
    //using intermediate buffer with size equal to first-level CPU cache size

    static const size_t TMP_BUF_SIZE = 4*1024 / sizeof(__m128i);
    __m128i tmpBuf[TMP_BUF_SIZE];
    //const size_t TMP_BUF_SIZE = (std::min<size_t>(4*1024, nv12UVPlaneSize)) / sizeof(__m128i);
    //__m128i* tmpBuf = (__m128i*)alloca(TMP_BUF_SIZE*sizeof(__m128i));

    static const __m128i MASK_HI = { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00 };
    static const __m128i AND_MASK = MASK_HI;
    static const __m128i SHIFT_8 = { 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    const __m128i* const nv12UVPlaneEnd = nv12UVPlane + nv12UVPlaneSize / sizeof(__m128i);
    while( nv12UVPlane < nv12UVPlaneEnd )
    {
        //reading to tmp buffer
        const size_t bytesToCopy = std::min<size_t>( (nv12UVPlaneEnd - nv12UVPlane)*sizeof(*nv12UVPlane), sizeof(tmpBuf) );
        //const size_t bytesToCopy = std::min<size_t>( (nv12UVPlaneEnd - nv12UVPlane)*sizeof(*nv12UVPlane), TMP_BUF_SIZE*sizeof(*tmpBuf) );
        memcpy_stream_load( tmpBuf, nv12UVPlane, bytesToCopy );
        nv12UVPlane += bytesToCopy / sizeof(*nv12UVPlane);

        //converting
        const __m128i* const tmpBufEnd = tmpBuf + bytesToCopy / sizeof(*tmpBuf);
        for( __m128i* nv12UVPlaneTmp = tmpBuf; nv12UVPlaneTmp < tmpBufEnd; )
        {
            __m128i x1 = *nv12UVPlaneTmp;
            __m128i x2 = *(nv12UVPlaneTmp+1);
            __m128i x3 = *(nv12UVPlaneTmp+2);
            __m128i x4 = *(nv12UVPlaneTmp+3);

            __m128i x5 = _mm_packus_epi16( _mm_and_si128( x1, AND_MASK ), _mm_and_si128( x2, AND_MASK ) );   //U
            __m128i x6 = _mm_packus_epi16( _mm_srl_epi16( x1, SHIFT_8 ), _mm_srl_epi16( x2, SHIFT_8 ) );   //V
            _mm_store_si128( yv12UPlane, x5 );
            _mm_store_si128( yv12VPlane, x6 );

            ++yv12UPlane;
            ++yv12VPlane;
            nv12UVPlaneTmp += 2;

            if( nv12UVPlaneTmp >= tmpBufEnd )
                break;

            x5 = _mm_packus_epi16( _mm_and_si128( x3, AND_MASK ), _mm_and_si128( x4, AND_MASK ) );   //U
            x6 = _mm_packus_epi16( _mm_srl_epi16( x3, SHIFT_8 ), _mm_srl_epi16( x4, SHIFT_8 ) );   //V
            _mm_store_si128( yv12UPlane, x5 );
            _mm_store_si128( yv12VPlane, x6 );

            ++yv12UPlane;
            ++yv12VPlane;
            nv12UVPlaneTmp += 2;
        }
    }
}
#endif

class ScopedAtomicLock
{
public:
    ScopedAtomicLock( QAtomicInt* const refCounter )
    :
        m_refCounter( refCounter )
    {
        m_refCounter->ref();
    }

    ~ScopedAtomicLock()
    {
        m_refCounter->deref();
    }

private:
    QAtomicInt* const m_refCounter;
};

/*!
    For now, it supports only DXVA textures with NV12 format
*/
class AsyncPicDataUploader
:
    public UploadFrameRunnable
{
    friend class UploadThread;

public:
    AsyncPicDataUploader( DecodedPictureToOpenGLUploader* uploader )
    :
        m_pictureBuf( NULL ),
        m_uploader( uploader ),
        m_yv12Buffer( NULL ),
        m_yv12BufferCapacity( 0 ),
        m_pts( 0 )
    {
        setAutoDelete( false );
        memset( m_planes, 0, sizeof(m_planes) );
        memset( m_lineSizes, 0, sizeof(m_lineSizes) );
    }

    ~AsyncPicDataUploader()
    {
        qFreeAligned( m_yv12Buffer );
        m_yv12Buffer = NULL;
    }

    virtual void run()
    {
        DecodedPictureToOpenGLUploader::UploadedPicture* const pictureBuf = m_pictureBuf;

        {
            QMutexLocker lk( &m_mutex );

            if( !m_pictureBuf.testAndSetOrdered( pictureBuf, NULL ) )
                return; //m_pictureBuf has been changed (running has been cancelled?)
            if( pictureBuf == NULL )
            {
                NX_LOG( QString::fromAscii("AsyncPicDataUploader. Picture upload has been cancelled..."), cl_logDEBUG1 );
                m_picDataRef.clear();
                m_uploader->pictureDataUploadCancelled( this );
                return; //running has been cancelled from outside
            }
        }

        QRect cropRect;
        if( !loadPicDataToMemBuffer( pictureBuf, &cropRect ) )
        {
            m_picDataRef.clear();
            m_uploader->pictureDataUploadFailed( this, pictureBuf );
            return;
        }

#ifdef GL_COPY_AGGREGATION
        if( !m_uploader->uploadDataToGlWithAggregation(
#else
        if( !m_uploader->uploadDataToGl(
#endif
                pictureBuf,
#ifdef RENDERER_SUPPORTS_NV12
                PIX_FMT_NV12,
#else
                PIX_FMT_YUV420P,
#endif
                cropRect.width(),
                cropRect.height(),
                m_planes,
                m_lineSizes,
                true ) )
        {
            NX_LOG( QString::fromAscii("AsyncPicDataUploader. Failed to move to opengl memory frame (pts %1) data. Skipping frame...").arg(pictureBuf->pts()), cl_logDEBUG1 );
            m_picDataRef.clear();
            m_uploader->pictureDataUploadFailed( this, pictureBuf );
            return;
        }

        m_picDataRef.clear();
        m_uploader->pictureDataUploadSucceeded( this, pictureBuf );
    }

    //!This method MUST NOT be called simultaneously with \a run. Synchronizing these methods is outside of this class
    void setData(
        DecodedPictureToOpenGLUploader::UploadedPicture* pictureBuf,
        const QSharedPointer<QnAbstractPictureDataRef>& picDataRef )
    {
        m_pictureBuf = pictureBuf;
        m_picDataRef = picDataRef;
        m_pts = pictureBuf->pts();
    }

    quint64 pts() const
    {
        return m_pts;
    }

    //!Cancelles uploading of picture: sets pointer to picture buffer to NULL and returns previous value
    //DecodedPictureToOpenGLUploader::UploadedPicture* cancel()
    //{
    //    return m_pictureBuf.fetchAndStoreOrdered( NULL );
    //}

    //!Returns true, if replace hjas been successfull and new picture will be uploaded. false otherwise
    bool replacePicture(
        unsigned int picSequence,
        const QSharedPointer<CLVideoDecoderOutput>& decodedPicture,
        const QSharedPointer<QnAbstractPictureDataRef>& picDataRef,
        quint64* const prevPicPts = NULL )
    {
        QMutexLocker lk( &m_mutex );

        if( !m_pictureBuf )
            return false;

        if( prevPicPts )
            *prevPicPts = m_pictureBuf->m_pts;
        m_pictureBuf->m_sequence = picSequence;
        m_pictureBuf->m_pts = decodedPicture->pkt_dts;
        m_pictureBuf->m_width = decodedPicture->width;
        m_pictureBuf->m_height = decodedPicture->height;
        m_pictureBuf->m_metadata = decodedPicture->metadata;
        m_picDataRef = picDataRef;

        return true;
    }

private:
    QAtomicPointer<DecodedPictureToOpenGLUploader::UploadedPicture> m_pictureBuf;
    QSharedPointer<QnAbstractPictureDataRef> m_picDataRef;
    DecodedPictureToOpenGLUploader* m_uploader;
    uint8_t* m_planes[3];
    int m_lineSizes[3];
    uint8_t* m_yv12Buffer;
    size_t m_yv12BufferCapacity;
    QMutex m_mutex;
    quint64 m_pts;

    /*!
        \return false, if method has been interrupted (m_picDataRef->isValid() returned false). true, otherwise
    */
    bool loadPicDataToMemBuffer( DecodedPictureToOpenGLUploader::UploadedPicture* const pictureBuf, QRect* const cropRect )
    {
#ifdef _WIN32
        //checking, if m_picDataRef ref has not been marked for released
        ScopedAtomicLock picUsageCounterLock( &m_picDataRef->syncCtx()->usageCounter );
        if( !m_picDataRef->isValid() )
        {
            NX_LOG( QString::fromAscii("AsyncPicDataUploader. Frame (pts %1, 0x%2) data ref has been invalidated (1). Releasing...").
                arg(pictureBuf->pts()).arg((size_t)m_picDataRef->syncCtx(), 0, 16), cl_logDEBUG1 );
            return false;
        }

        Q_ASSERT( m_picDataRef->type() == QnAbstractPictureDataRef::pstD3DSurface );
        IDirect3DSurface9* const surf = static_cast<D3DPictureData*>(m_picDataRef.data())->getSurface();
        D3DSURFACE_DESC surfDesc;
        memset( &surfDesc, 0, sizeof(surfDesc) );
        HRESULT res = surf->GetDesc( &surfDesc );
        if( res != D3D_OK )
        {
            NX_LOG( QString::fromAscii("AsyncPicDataUploader. Failed to get dxva surface info (%1). Ignoring decoded picture...").arg(res), cl_logERROR );
            return false;
        }

        if( surfDesc.Format != (D3DFORMAT)MAKEFOURCC('N','V','1','2') )
        {
            NX_LOG( QString::fromAscii("AsyncPicDataUploader. Dxva surface format %1 while only NV12 (%2) is supported. Ignoring decoded picture...").
                arg(surfDesc.Format).arg(MAKEFOURCC('N','V','1','2')), cl_logERROR );
            return false;
        }

        *cropRect = m_picDataRef->cropRect();

#ifndef DISABLE_FRAME_DOWNLOAD
        D3DLOCKED_RECT lockedRect; 
        memset( &lockedRect, 0, sizeof(lockedRect) );
        RECT rectToLock;
        memset( &rectToLock, 0, sizeof(rectToLock) );
        rectToLock.left = cropRect->left();
        rectToLock.top = cropRect->top();
        rectToLock.right = cropRect->right() + 1;
        rectToLock.bottom = cropRect->bottom() + 1;
        res = surf->LockRect( &lockedRect, &rectToLock, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY );
        if( res != D3D_OK )
        {
            NX_LOG( QString::fromAscii("AsyncPicDataUploader. Failed to map dxva surface (%1). Ignoring decoded picture...").
                arg(QString::fromWCharArray(DXGetErrorDescription(res))), cl_logERROR );
            return false;
        }
#endif

        //using crop rect of frame

        const unsigned int targetPitch = minPow2( cropRect->width() );
        const unsigned int targetHeight = cropRect->height();

        //converting to YV12
        if( m_yv12BufferCapacity < targetHeight * targetPitch * 3 / 2 )
        {
            m_yv12BufferCapacity = targetHeight * targetPitch * 3 / 2;
            qFreeAligned( m_yv12Buffer );
            m_yv12Buffer = (uint8_t*)qMallocAligned( m_yv12BufferCapacity, sizeof(__m128i) );
            if( !m_yv12Buffer )
            {
#ifndef DISABLE_FRAME_DOWNLOAD
                surf->UnlockRect();
#endif
                NX_LOG( QString::fromAscii("AsyncPicDataUploader. Frame (pts %1, 0x%2) could not be uploaded due to memory allocation error. Releasing...").
                    arg(pictureBuf->pts()).arg((size_t)m_picDataRef->syncCtx(), 0, 16), cl_logDEBUG1 );
                return false;
            }
        }
        m_planes[0] = m_yv12Buffer;               //Y-plane
        m_lineSizes[0] = targetPitch;
#ifdef RENDERER_SUPPORTS_NV12
        m_planes[1] = m_yv12Buffer + targetHeight * targetPitch;          //UV-plane
        m_lineSizes[1] = targetPitch;
#else
        m_planes[1] = m_yv12Buffer + targetHeight * targetPitch;          //U-plane
        m_lineSizes[1] = targetPitch / 2;  
        m_planes[2] = m_yv12Buffer + targetHeight * targetPitch / 4 * 5;  //V-plane
        m_lineSizes[2] = targetPitch / 2;
#endif

#ifdef DISABLE_FRAME_DOWNLOAD
        return true;
#else

        //Y-plane
        if( !copyImageRectStreamLoad(
                *m_picDataRef.data(),
                (quint8*)m_planes[0],
                cropRect->width(),
                cropRect->height(),
                targetPitch,
                (const quint8*)lockedRect.pBits,
                lockedRect.Pitch ) )
        {
            surf->UnlockRect();
            NX_LOG( QString::fromAscii("AsyncPicDataUploader. Frame (pts %1, 0x%2) data ref has been invalidated (2). Releasing...").
                arg(pictureBuf->pts()).arg((size_t)m_picDataRef->syncCtx(), 0, 16), cl_logDEBUG1 );
            return false;
        }
#ifdef RENDERER_SUPPORTS_NV12
        if( !copyImageRectStreamLoad(
                *m_picDataRef.data(),
                (quint8*)m_planes[1],
                cropRect->width(),       //X resolution is 1/2, but every pixel has 2 bytes
                cropRect->height() / 2,  //Y resolution is 1/2, so number of lines is 2 times less
                targetPitch,
                (const quint8*)lockedRect.pBits + lockedRect.Pitch * surfDesc.Height,
                lockedRect.Pitch ) )
        {
            surf->UnlockRect();
            NX_LOG( QString::fromAscii("AsyncPicDataUploader. Frame (pts %1, 0x%2) data ref has been invalidated (3). Releasing...").
                arg(pictureBuf->pts()).arg((size_t)m_picDataRef->syncCtx(), 0, 16), cl_logDEBUG1 );
            return false;
        }
#else
        if( !streamLoadAndDeinterleaveNV12UVPlaneEx(
                *m_picDataRef.data(),
                (quint8*)lockedRect.pBits + lockedRect.Pitch * surfDesc.Height,
                cropRect->width(),
                cropRect->height(),
                lockedRect.Pitch,
                (quint8*)m_planes[1],
                (quint8*)m_planes[2],
                targetPitch / 2 ) )
        {
            surf->UnlockRect();
            NX_LOG( QString::fromAscii("AsyncPicDataUploader. Frame (pts %1, 0x%2) data ref has been invalidated (4). Releasing...").
                arg(pictureBuf->pts()).arg((size_t)m_picDataRef->syncCtx(), 0, 16), cl_logDEBUG1 );
            return false;
        }
#endif

        surf->UnlockRect();

        return true;
#endif  //DISABLE_FRAME_DOWNLOAD
#else
        Q_UNUSED( pictureBuf );
        Q_UNUSED( cropRect );
        Q_ASSERT( false );
        return false;
#endif  //_WIN32
    }

#ifdef _WIN32
    inline bool copyImageRectStreamLoad(
        const QnAbstractPictureDataRef& picDataRef,
        quint8* dstMem,
        unsigned int dstWidth,
        unsigned int dstHeight,
        unsigned int dstPitch,
        const quint8* srcMem,
        unsigned int srcPitch )
    {
        static const unsigned int PIXEL_SIZE = 1;
        for( unsigned int y = 0; y < dstHeight; ++y )
        {
            memcpy_stream_load( (__m128i*)(dstMem+y*dstPitch), (__m128i*)(srcMem+y*srcPitch), dstWidth*PIXEL_SIZE );
            if( !picDataRef.isValid() )
                return false;
        }

        return true;
    }

    /*!
        Deinterleaves NV12 UV-plane rect (\a srcWidth, \a srcHeight) starting at \a srcMem to \a dstUPlane and \a dstVPlane
    */
    inline bool streamLoadAndDeinterleaveNV12UVPlaneEx(
        const QnAbstractPictureDataRef& picDataRef,
        const quint8* srcMem,
        unsigned int /*srcWidth*/,
        unsigned int srcHeight,
        size_t srcPitch,
        quint8* dstUPlane,
        quint8* dstVPlane,
        size_t dstPitch )
    {
        for( unsigned int y = 0; y < srcHeight / 2; ++y )
        {
            streamLoadAndDeinterleaveNV12UVPlane(
                (__m128i*)(srcMem + y*srcPitch),
                dstPitch * 2,   //not using srcPitch here because we deinterleave only one not full line
                (__m128i*)(dstUPlane + y*dstPitch),
                (__m128i*)(dstVPlane + y*dstPitch) );
            if( !picDataRef.isValid() )
                return false;
        }

        return true;
    }
#endif
};


//////////////////////////////////////////////////////////
// AVPacketUploader
//////////////////////////////////////////////////////////
class AVPacketUploader
:
    public UploadFrameRunnable
{
public:
    AVPacketUploader(
        DecodedPictureToOpenGLUploader::UploadedPicture* const dest,
        const QSharedPointer<CLVideoDecoderOutput>& src,
        DecodedPictureToOpenGLUploader* uploader )
    :
        m_dest( dest ),
        m_src( src ),
        m_uploader( uploader ),
        m_isRunning( false ),
        m_done( false ),
        m_success( false )
    {
        setAutoDelete( false );
#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
        //m_src->setDisplaying( true );
#endif
    }

    ~AVPacketUploader()
    {
#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
        //m_src->setDisplaying( false );
#endif
    }

    virtual void run()
    {
        m_isRunning = true;

        m_success = 
#ifdef GL_COPY_AGGREGATION
            m_uploader->uploadDataToGlWithAggregation(
#else
            m_uploader->uploadDataToGl(
#endif
                m_dest,
                (PixelFormat)m_src->format,
                m_src->width,
                m_src->height,
                m_src->data,
                m_src->linesize,
                false );
        m_done = true;
        if( m_success )
            m_uploader->pictureDataUploadSucceeded( NULL, m_dest );
        else
            m_uploader->pictureDataUploadFailed( NULL, m_dest );

        m_isRunning = false;
    }

    bool done() const
    {
        return m_done;
    }

    bool success() const
    {
        return m_success;
    }

    const QSharedPointer<CLVideoDecoderOutput>& decodedFrame() const
    {
        return m_src;
    }

    DecodedPictureToOpenGLUploader::UploadedPicture* picture()
    {
        return m_dest;
    }

    void markAsRunning()
    {
        m_isRunning = true;
    }

    bool isRunning() const
    {
        return m_isRunning;
    }

private:
    DecodedPictureToOpenGLUploader::UploadedPicture* const m_dest;
    QSharedPointer<CLVideoDecoderOutput> m_src;
    DecodedPictureToOpenGLUploader* m_uploader;
    bool m_isRunning;
    bool m_done;
    bool m_success;
};


//////////////////////////////////////////////////////////
// DecodedPicturesDeleter
//////////////////////////////////////////////////////////
class DecodedPicturesDeleter
:
    public UploadFrameRunnable
{
public:
    DecodedPicturesDeleter( DecodedPictureToOpenGLUploader* uploader )
    :
        m_uploader( uploader )
    {
        setAutoDelete( true );
    }

    virtual void run()
    {
        m_uploader->releasePictureBuffers();
    }

private:
    DecodedPictureToOpenGLUploader* const m_uploader;
};


//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploader
//////////////////////////////////////////////////////////

//TODO/IMPL minimize blocking in \a DecodedPictureToOpenGLUploader::uploadDataToGl method: 
    //before calling this method we can check that it would not block and take another task if it would

//TODO/IMPL use nv12->rgba shader

//we need second frame in case of using async upload to ensure renderer always gets something to draw
static const size_t MIN_GL_PIC_BUF_COUNT = 2;

#ifdef SINGLE_STREAM_UPLOAD
static DecodedPictureToOpenGLUploader* runningUploader = NULL;
#endif

DecodedPictureToOpenGLUploader::DecodedPictureToOpenGLUploader(
    const QGLContext* const mainContext,
    unsigned int asyncDepth )
:
    d( new DecodedPictureToOpenGLUploaderPrivate(mainContext) ),
    m_format( PIX_FMT_NONE ),
    m_yuv2rgbBuffer( NULL ),
    m_yuv2rgbBufferLen( 0 ),
    m_forceSoftYUV( false ),
    m_painterOpacity( 1.0 ),
    m_previousPicSequence( 1 ),
    m_terminated( false ),
    m_yv12SharedUsed( false ),
    m_nv12SharedUsed( false ),
    m_uploadThread( NULL ),
    m_rgbaBuf( NULL ),
    m_fileNumber( 0 ),
    m_hardwareDecoderUsed( false ),
    m_asyncUploadUsed( false ),
    m_initializedCtx( NULL )
{
#ifdef ASYNC_UPLOADING_USED
    const std::vector<QSharedPointer<DecodedPictureToOpenGLUploadThread> >& 
        pool = DecodedPictureToOpenGLUploaderContextPool::instance()->getPoolOfContextsSharedWith( mainContext );
    Q_ASSERT( !pool.empty() );

    m_uploadThread = pool[random(0, pool.size())];    //TODO/IMPL should take 
#endif

    for( size_t i = 0; i < asyncDepth+MIN_GL_PIC_BUF_COUNT; ++i )
        m_emptyBuffers.push_back( new UploadedPicture( this ) );

#ifdef SINGLE_STREAM_UPLOAD
    if( !runningUploader )
        runningUploader = this;
#endif
}

DecodedPictureToOpenGLUploader::~DecodedPictureToOpenGLUploader()
{
    //ensure there is no pointer to the object in the async uploader queue
    discardAllFramesPostedToDisplay();

    pleaseStop();

    for( std::deque<AsyncPicDataUploader*>::iterator
        it = m_unusedUploaders.begin();
        it != m_unusedUploaders.end();
        ++it )
    {
        delete *it;
    }
    m_unusedUploaders.clear();

    {
        //have to remove buffers in uploading thread, since it holds gl context current
        QMutexLocker lk( &m_mutex );
#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
        if( !m_asyncUploadUsed )
        {
        }
        else
#endif
#ifdef ASYNC_UPLOADING_USED
        m_uploadThread->push( new DecodedPicturesDeleter( this ) );
#else
            {}
#endif
        while( !(m_emptyBuffers.empty() && m_renderedPictures.empty() && m_picturesWaitingRendering.empty() && m_picturesBeingRendered.empty()) )
        {
            m_cond.wait( lk.mutex() );
        }
    }

#ifdef SINGLE_STREAM_UPLOAD
    if( runningUploader == this )
        runningUploader = NULL;
#endif

#ifdef ASYNC_UPLOADING_USED
    //unbinding uploading thread
    m_uploadThread.clear();
#endif

    delete[] m_rgbaBuf;
}

void DecodedPictureToOpenGLUploader::pleaseStop()
{
    QMutexLocker lk( &m_mutex );

    if( m_terminated )
        return;

    m_terminated = true;

#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
    if( !m_asyncUploadUsed )
    {
        cancelUploadingInGUIThread();   //NOTE this method cannot block if called from GUI thread, so we will never hang GUI thread if called in it

        //TODO/IMPL have to remove gl textures now, while QGLContext surely alive
        while( !m_picturesBeingRendered.empty() )
            m_cond.wait( lk.mutex() );

        const QGLContext* curCtx = QGLContext::currentContext();
        if( curCtx != m_initializedCtx && m_initializedCtx )
            m_initializedCtx->makeCurrent();
        releasePictureBuffersNonSafe();
        if( curCtx != m_initializedCtx && m_initializedCtx )
        {
            m_initializedCtx->doneCurrent();
            if( curCtx )
                const_cast<QGLContext*>(curCtx)->makeCurrent();
        }
    }
#endif

    m_cond.wakeAll();
}

void DecodedPictureToOpenGLUploader::uploadDecodedPicture( const QSharedPointer<CLVideoDecoderOutput>& decodedPicture )
{
    NX_LOG( QString::fromAscii( "Uploading decoded picture to gl textures. dts %1" ).arg(decodedPicture->pkt_dts), cl_logDEBUG2 );

    m_hardwareDecoderUsed = decodedPicture->flags & QnAbstractMediaData::MediaFlags_HWDecodingUsed;

#ifdef UPLOAD_SYSMEM_FRAMES_ASYNC
    m_asyncUploadUsed = !decodedPicture->picData.data() || (decodedPicture->picData->type() != QnAbstractPictureDataRef::pstOpenGL);
#else
    m_asyncUploadUsed = decodedPicture->picData.data() && (decodedPicture->picData->type() == QnAbstractPictureDataRef::pstD3DSurface);
#endif
    m_format = decodedPicture->format;
    UploadedPicture* emptyPictureBuf = NULL;

    QMutexLocker lk( &m_mutex );

    if( m_terminated )
        return;

    {
        //searching for a non-used picture buffer
        for( ;; )
        {
            if( m_terminated )
                return;

#if defined(UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD) && defined(USE_MIN_GL_TEXTURES)
            if( !m_asyncUploadUsed && !m_hardwareDecoderUsed && !m_renderedPictures.empty() )
            {
                //this condition allows to use single PictureBuffer for rendering
                emptyPictureBuf = m_renderedPictures.front();
                m_renderedPictures.pop_front();
                NX_LOG( QString::fromAscii( "Taking (1) rendered picture (pts %1) buffer for upload (pts %2). (%3, %4)" ).
                    arg(emptyPictureBuf->pts()).arg(decodedPicture->pkt_dts).arg(m_renderedPictures.size()).arg(m_picturesWaitingRendering.size()), cl_logDEBUG2 );
            }
            else
#endif
            if( !m_emptyBuffers.empty() )
            {
                emptyPictureBuf = m_emptyBuffers.front();
                m_emptyBuffers.pop_front();
                NX_LOG( QString::fromAscii( "Found empty buffer" ), cl_logDEBUG2 );
            }
            else if( (!m_asyncUploadUsed && !m_renderedPictures.empty())
                  || (m_asyncUploadUsed && (m_renderedPictures.size() > (m_picturesWaitingRendering.empty() ? 1U : 0U))) )  //reserving one uploaded picture (preferring picture 
                                                                                                                       //which has not been shown yet) so that renderer always 
                                                                                                                       //gets something to draw...
            {
                //selecting oldest rendered picture
                emptyPictureBuf = m_renderedPictures.front();
                m_renderedPictures.pop_front();
                NX_LOG( QString::fromAscii( "Taking (2) rendered picture (pts %1) buffer for upload (pts %2). (%3, %4)" ).
                    arg(emptyPictureBuf->pts()).arg(decodedPicture->pkt_dts).arg(m_renderedPictures.size()).arg(m_picturesWaitingRendering.size()), cl_logDEBUG2 );
            }
            else if( ((!m_asyncUploadUsed && !m_picturesWaitingRendering.empty())
                       || (m_asyncUploadUsed && (m_picturesWaitingRendering.size() > (m_renderedPictures.empty() ? 1U : 0U))))
                      && !m_picturesWaitingRendering.front()->m_skippingForbidden )
            {
                //looks like rendering does not catch up with decoding. Ignoring oldest decoded frame...
                emptyPictureBuf = m_picturesWaitingRendering.front();
                m_picturesWaitingRendering.pop_front();
                NX_LOG( QString::fromAscii( "Ignoring uploaded frame with pts %1. Playback does not catch up with uploading. (%2, %3)..." ).
                    arg(emptyPictureBuf->pts()).arg(m_renderedPictures.size()).arg(m_picturesWaitingRendering.size()), cl_logDEBUG1 );
            }
#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
            else if( !m_asyncUploadUsed && !m_framesWaitingUploadInGUIThread.empty() )
            {
                for( std::deque<AVPacketUploader*>::iterator
                    it = m_framesWaitingUploadInGUIThread.begin();
                    it != m_framesWaitingUploadInGUIThread.end();
                    ++it )
                {
                    if( (*it)->isRunning() || (*it)->picture()->m_skippingForbidden )
                        continue;
                    NX_LOG( QString::fromAscii( "Ignoring decoded frame with timestamp %1 (%2). Playback does not catch up with decoding" ).
                        arg((*it)->picture()->m_pts).arg(QDateTime::fromMSecsSinceEpoch((*it)->picture()->m_pts/1000).toString(QLatin1String("hh:mm:ss.zzz"))), cl_logDEBUG2 );
                    emptyPictureBuf = (*it)->picture();
                    delete (*it);
                    m_framesWaitingUploadInGUIThread.erase( it );
                    break;
                }
            }
#endif

            if( emptyPictureBuf == NULL )
            {
                if( m_asyncUploadUsed )
                {
                    //replacing newest decoded frame in the queue with new one...
                    if( !m_usedUploaders.empty() )
                    {
                        quint64 prevPicPts = 0;
                        if( m_usedUploaders.back()->replacePicture( nextPicSequenceValue(), decodedPicture, decodedPicture->picData, &prevPicPts ) )
                        {
                            NX_LOG( QString::fromAscii( "Cancelled upload of decoded frame with pts %1 in favor of frame with pts %2" ).
                                arg(prevPicPts).arg(decodedPicture->pkt_dts), cl_logDEBUG1 );
                            decodedPicture->picData.clear();
                            return;
                        }
                    }

                    //ignoring decoded picture so that not to stop decoder
                    NX_LOG( QString::fromAscii( "Ignoring decoded frame with pts %1. Uploading does not catch up with decoding..." ).arg(decodedPicture->pkt_dts), cl_logDEBUG1 );
                    decodedPicture->picData.clear();
                    return;
                }
                NX_LOG( QString::fromAscii( "Waiting for a picture gl buffer to get free" ), cl_logDEBUG1 );
                //waiting for a picture buffer to get free
                m_cond.wait( lk.mutex() );
                continue;
            }
            break;
        }
    }

    //copying attributes of decoded picture
    emptyPictureBuf->m_sequence = nextPicSequenceValue();
    emptyPictureBuf->m_pts = decodedPicture->pkt_dts;
    emptyPictureBuf->m_width = decodedPicture->width;
    emptyPictureBuf->m_height = decodedPicture->height;
    emptyPictureBuf->m_metadata = decodedPicture->metadata;
    emptyPictureBuf->m_flags = decodedPicture->flags;
    emptyPictureBuf->m_skippingForbidden = false;

    if( decodedPicture->picData )
    {
#ifdef _WIN32
        if( decodedPicture->picData->type() == QnAbstractPictureDataRef::pstD3DSurface )
        {
            //posting picture to worker thread

            AsyncPicDataUploader* uploaderToUse = NULL;
            if( m_unusedUploaders.empty() )
            {
                uploaderToUse = new AsyncPicDataUploader( this );
            }
            else
            {
                uploaderToUse = m_unusedUploaders.front();
                m_unusedUploaders.pop_front();
            }

            uploaderToUse->setData( emptyPictureBuf, decodedPicture->picData );
            m_usedUploaders.push_back( uploaderToUse );
            decodedPicture->picData.clear();    //releasing no more needed reference to picture data
            m_uploadThread->push( uploaderToUse );
        }
        else
#endif
        if( decodedPicture->picData->type() == QnAbstractPictureDataRef::pstOpenGL )
        {
            //TODO/IMPL save reference to existing opengl texture
            Q_ASSERT( false );
        }
        else
        {
            Q_ASSERT( false );
        }
    }
    else    //data is stored in system memory (in AVPacket)
    {
        //have go through upload thread, since opengl uploading does not scale good on Intel HD Graphics and 
            //it does not matter on PCIe graphics card due to high video memory bandwidth

#ifdef UPLOAD_SYSMEM_FRAMES_ASYNC
        QSharedPointer<CLVideoDecoderOutput> decodedPictureCopy( new CLVideoDecoderOutput() );
        decodedPictureCopy->reallocate( decodedPicture->width, decodedPicture->height, decodedPicture->format );
        CLVideoDecoderOutput::copy( decodedPicture.data(), decodedPictureCopy.data() );
        std::auto_ptr<AVPacketUploader> avPacketUploader( new AVPacketUploader( emptyPictureBuf, decodedPictureCopy, this ) );
        avPacketUploader->setAutoDelete( true );
        m_uploadThread->push( avPacketUploader.release() );
#elif defined(UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD)
        m_framesWaitingUploadInGUIThread.push_back( new AVPacketUploader( emptyPictureBuf, decodedPicture, this ) );
#else
        //using uploading thread, blocking till upload is completed
        std::auto_ptr<AVPacketUploader> avPacketUploader( new AVPacketUploader( emptyPictureBuf, decodedPicture, this ) );
        m_uploadThread->push( avPacketUploader.get() );
        while( !avPacketUploader->done() )
        {
            m_cond.wait( lk.mutex() );
        }
#endif

        //savePicToFile( decodedPicture.data(), decodedPicture->pkt_dts );

        //if( avPacketUploader->success() )
        //    m_picturesWaitingRendering.push_back( emptyPictureBuf );
        //else
        //    m_emptyBuffers.push_back( emptyPictureBuf );        //considering picture buffer invalid
    }
}

bool DecodedPictureToOpenGLUploader::isUsingFrame( const QSharedPointer<CLVideoDecoderOutput>& image ) const
{
    QMutexLocker lk( &m_mutex );

#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
    foreach( AVPacketUploader* uploader, m_framesWaitingUploadInGUIThread )
    {
        if( uploader->decodedFrame() == image )
            return true;
    }
#endif

    return false;
}

DecodedPictureToOpenGLUploader::UploadedPicture* DecodedPictureToOpenGLUploader::getUploadedPicture() const
{
    QMutexLocker lk( &m_mutex );

    if( m_terminated )
        return NULL;

#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
    if( !m_framesWaitingUploadInGUIThread.empty() )
    {
        //uploading
        AVPacketUploader* frameUploader = m_framesWaitingUploadInGUIThread.front();
        frameUploader->markAsRunning();
        frameUploader->picture()->m_skippingForbidden = true;   //otherwise, we could come to situation that there is no data to display
        lk.unlock();
        frameUploader->run();
        lk.relock();
        m_framesWaitingUploadInGUIThread.pop_front();
        delete frameUploader;
        m_cond.wakeAll();   //notifying that upload is completed
    }
#endif

    UploadedPicture* pic = NULL;
    if( !m_picturesWaitingRendering.empty() )
    {
        pic = m_picturesWaitingRendering.front();
#ifdef SYNC_UPLOADING_WITH_GLFENCE
        if( !pic->m_glFence.trySync() )
            return NULL;
#endif
        m_picturesWaitingRendering.pop_front();
        NX_LOG( QString::fromAscii( "Taking uploaded picture (pts %1, seq %2) for first-time rendering" ).arg(pic->pts()).arg(pic->m_sequence), cl_logDEBUG2 );
    }
    else if( !m_renderedPictures.empty() )
    {
        //displaying previous displayed frame, since no new frame available...
        pic = m_renderedPictures.back();
#ifdef SYNC_UPLOADING_WITH_GLFENCE
        if( !pic->m_glFence.trySync() )
            return NULL;
#endif
        m_renderedPictures.pop_back();
        NX_LOG( QString::fromAscii( "Taking previously shown uploaded picture (pts %1, seq %2) for rendering" ).arg(pic->pts()).arg(pic->m_sequence), cl_logDEBUG2 );
    }
    else
    {
        NX_LOG( QString::fromAscii( "Failed to find picture for rendering. No data from decoder?" ), cl_logWARNING );
        return NULL;
    }

    m_picturesBeingRendered.push_back( pic );
    lk.unlock();

#if defined(GL_COPY_AGGREGATION) && defined(UPLOAD_TO_GL_IN_GUI_THREAD) && !defined(DISABLE_FRAME_UPLOAD)
    //if needed, uploading aggregation surface to ogl texture(s)
    pic->aggregationSurfaceRect()->ensureUploadedToOGL( opacity() );
#endif

    //waiting for all gl operations, submitted by uploader are completed by ogl device
    return pic;
}

quint64 DecodedPictureToOpenGLUploader::nextFrameToDisplayTimestamp() const
{
    QMutexLocker lk( &m_mutex );

    if( !m_picturesBeingRendered.empty() )
        return m_picturesBeingRendered.front()->pts();
    if( !m_picturesWaitingRendering.empty() )
        return m_picturesWaitingRendering.front()->pts();
#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
    if( !m_framesWaitingUploadInGUIThread.empty() )
        return m_framesWaitingUploadInGUIThread.front()->picture()->pts();
#endif
    //async upload case (only when HW decoding enabled)
    if( !m_usedUploaders.empty() )
        return m_usedUploaders.front()->pts();
    if( !m_renderedPictures.empty() )
        return m_renderedPictures.back()->pts();
    return AV_NOPTS_VALUE;
}

void DecodedPictureToOpenGLUploader::waitForAllFramesDisplayed()
{
    QMutexLocker lk( &m_mutex );

    while( !m_terminated && (!m_framesWaitingUploadInGUIThread.empty() || !m_picturesWaitingRendering.empty() || !m_usedUploaders.empty() || !m_picturesBeingRendered.empty()) )
        m_cond.wait( lk.mutex() );
}

void DecodedPictureToOpenGLUploader::ensureAllFramesWillBeDisplayed()
{
    if( m_hardwareDecoderUsed )
        return; //if hardware decoder is used, waiting can result in bad playback experience

    QMutexLocker lk( &m_mutex );

#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
    //MUST wait till all references to frames, supplied via uploadDecodedPicture are not needed anymore
    while( !m_terminated && !m_framesWaitingUploadInGUIThread.empty() )
        m_cond.wait( lk.mutex() );

    //for( std::deque<AVPacketUploader*>::iterator
    //    it = m_framesWaitingUploadInGUIThread.begin();
    //    it != m_framesWaitingUploadInGUIThread.end();
    //    ++it )
    //{
    //    (*it)->picture()->m_skippingForbidden = true;
    //}
#endif
#ifdef UPLOAD_SYSMEM_FRAMES_ASYNC
    #error "Not implemented"
#endif

    //marking, that skipping frames currently in queue is forbidden and exiting...
    for( std::deque<UploadedPicture*>::iterator
        it = m_picturesWaitingRendering.begin();
        it != m_picturesWaitingRendering.end();
        ++it )
    {
        (*it)->m_skippingForbidden = true;
    }
}

#ifdef GL_COPY_AGGREGATION
static DecodedPictureToOpenGLUploader::UploadedPicture* resetAggregationSurfaceRect( DecodedPictureToOpenGLUploader::UploadedPicture* pic )
{
    pic->setAggregationSurfaceRect( QSharedPointer<AggregationSurfaceRect>() );
    return pic;
}
#endif

void DecodedPictureToOpenGLUploader::discardAllFramesPostedToDisplay()
{
    QMutexLocker lk( &m_mutex );

    while( !m_usedUploaders.empty() )
        m_cond.wait( lk.mutex() );

#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
    cancelUploadingInGUIThread();
#endif

	for( std::deque<UploadedPicture*>::iterator
		it = m_picturesWaitingRendering.begin();
		it != m_picturesWaitingRendering.end() && !m_picturesWaitingRendering.empty();
         )
    {
        m_emptyBuffers.push_back( *it );
        it = m_picturesWaitingRendering.erase( it );
    }
}

void DecodedPictureToOpenGLUploader::cancelUploadingInGUIThread()
{
    //NOTE m_mutex MUST be locked!

    while( !m_framesWaitingUploadInGUIThread.empty() )
    {
        for( std::deque<AVPacketUploader*>::iterator
            it = m_framesWaitingUploadInGUIThread.begin();
            it != m_framesWaitingUploadInGUIThread.end();
            )
        {
            if( (*it)->isRunning() )
            {
                ++it;
                continue;
            }
            m_emptyBuffers.push_back( (*it)->picture() );
            delete *it;
            it = m_framesWaitingUploadInGUIThread.erase( it );
        }

        if( m_framesWaitingUploadInGUIThread.empty() )
            break;
        m_cond.wait( &m_mutex );
    }
}

void DecodedPictureToOpenGLUploader::waitForCurrentFrameDisplayed()
{
    QMutexLocker lk( &m_mutex );

    while( !m_terminated && (!m_picturesBeingRendered.empty()) )
        m_cond.wait( lk.mutex() );
}

void DecodedPictureToOpenGLUploader::pictureDrawingFinished( UploadedPicture* const picture ) const
{
#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
    if( m_hardwareDecoderUsed )
#endif
    picture->m_glFence.placeFence();

    QMutexLocker lk( &m_mutex );

    NX_LOG( QString::fromAscii( "Finished rendering of picture (pts %1)" ).arg(picture->pts()), cl_logDEBUG2 );

    //m_picturesBeingRendered holds only one picture
    std::deque<UploadedPicture*>::iterator it = std::find( m_picturesBeingRendered.begin(), m_picturesBeingRendered.end(), picture );
    Q_ASSERT( it != m_picturesBeingRendered.end() );

#ifdef GL_COPY_AGGREGATION
    //clearing all rendered pictures. Only one rendered picture is required (so that there is always anything to show)
    std::transform(
        m_renderedPictures.begin(),
        m_renderedPictures.end(),
        std::back_inserter(m_emptyBuffers),
        resetAggregationSurfaceRect );
    m_renderedPictures.clear();
#endif
    m_renderedPictures.push_back( *it );
    m_picturesBeingRendered.erase( it );
    m_cond.wakeAll();
}

void DecodedPictureToOpenGLUploader::setForceSoftYUV( bool value )
{
    m_forceSoftYUV = value;
}

bool DecodedPictureToOpenGLUploader::isForcedSoftYUV() const
{
    return m_forceSoftYUV;
}

qreal DecodedPictureToOpenGLUploader::opacity() const
{
    return m_painterOpacity;
}

void DecodedPictureToOpenGLUploader::setOpacity( qreal opacity )
{
    m_painterOpacity = opacity;
}

void DecodedPictureToOpenGLUploader::beforeDestroy()
{
    pleaseStop();
}

void DecodedPictureToOpenGLUploader::setYV12ToRgbShaderUsed( bool yv12SharedUsed )
{
    m_yv12SharedUsed = yv12SharedUsed;
}

void DecodedPictureToOpenGLUploader::setNV12ToRgbShaderUsed( bool nv12SharedUsed )
{
    m_nv12SharedUsed = nv12SharedUsed;
}

void DecodedPictureToOpenGLUploader::pictureDataUploadSucceeded( AsyncPicDataUploader* const uploader, UploadedPicture* const picture )
{
    QMutexLocker lk( &m_mutex );
    m_picturesWaitingRendering.push_back( picture );

    if( uploader )
    {
        m_usedUploaders.erase( std::find( m_usedUploaders.begin(), m_usedUploaders.end(), uploader ) );
        m_unusedUploaders.push_back( uploader );
    }
    m_cond.wakeAll();   //notifying that uploading finished
}

void DecodedPictureToOpenGLUploader::pictureDataUploadFailed( AsyncPicDataUploader* const uploader, UploadedPicture* const picture )
{
    QMutexLocker lk( &m_mutex );
    //considering picture buffer invalid
    if( picture )
    {
#ifdef GL_COPY_AGGREGATION
        picture->setAggregationSurfaceRect( QSharedPointer<AggregationSurfaceRect>() );
#endif
        m_emptyBuffers.push_back( picture );
    }

    if( uploader )
    {
        m_usedUploaders.erase( std::find( m_usedUploaders.begin(), m_usedUploaders.end(), uploader ) );
        m_unusedUploaders.push_back( uploader );
    }
    m_cond.wakeAll();   //notifying that uploading finished
}

void DecodedPictureToOpenGLUploader::pictureDataUploadCancelled( AsyncPicDataUploader* const uploader )
{
    QMutexLocker lk( &m_mutex );
    if( uploader )
    {
        m_usedUploaders.erase( std::find( m_usedUploaders.begin(), m_usedUploaders.end(), uploader ) );
        m_unusedUploaders.push_back( uploader );
    }
    m_cond.wakeAll();   //notifying that uploading finished
}

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
                return GL_BGR_EXT;
            default:
                break;
        }
    }
    return GL_RGBA;
}

#ifdef USE_PBO
inline void memcpy_sse4_stream_stream( __m128i* dst, __m128i* src, size_t sz )
{
    const __m128i* const src_end = src + sz / sizeof(__m128i);
    while( src < src_end )
    {
         __m128i x1 = _mm_stream_load_si128( src );
         __m128i x2 = _mm_stream_load_si128( src+1 );
         __m128i x3 = _mm_stream_load_si128( src+2 );
         __m128i x4 = _mm_stream_load_si128( src+3 );

         src += 4;

         _mm_stream_si128( dst, x1 );
         _mm_stream_si128( dst+1, x2 );
         _mm_stream_si128( dst+2, x3 );
         _mm_stream_si128( dst+3, x4 );

         dst += 4;
    }
}
inline void memcpy_sse4_stream_store( __m128i* dst, __m128i* src, size_t sz )
{
    const __m128i* const src_end = src + sz / sizeof(__m128i);
    while( src < src_end )
    {
         __m128i x1 = _mm_load_si128( src );
         __m128i x2 = _mm_load_si128( src+1 );
         __m128i x3 = _mm_load_si128( src+2 );
         __m128i x4 = _mm_load_si128( src+3 );

         src += 4;

         _mm_stream_si128( dst, x1 );
         _mm_stream_si128( dst+1, x2 );
         _mm_stream_si128( dst+2, x3 );
         _mm_stream_si128( dst+3, x4 );

         dst += 4;
    }
}
#endif

bool DecodedPictureToOpenGLUploader::uploadDataToGl(
    DecodedPictureToOpenGLUploader::UploadedPicture* const emptyPictureBuf,
    const PixelFormat format,
    const unsigned int width,
    const unsigned int height,
    uint8_t* planes[],
    int lineSizes[],
    bool /*isVideoMemory*/ )
{
#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
    if( !m_initializedCtx && !m_asyncUploadUsed )
        m_initializedCtx = const_cast<QGLContext*>(QGLContext::currentContext());
#endif

    //NX_LOG( QString::fromLatin1("DecodedPictureToOpenGLUploader::uploadDataToGl. %1").arg((size_t)this), cl_logINFO );

    //waiting for all operations with textures (submitted by renderer) are finished
    //emptyPictureBuf->m_glFence.sync();

#ifdef DISABLE_FRAME_UPLOAD
    emptyPictureBuf->setColorFormat( PIX_FMT_YUV420P );
    return true;
#endif

#ifdef SINGLE_STREAM_UPLOAD
    if( this != runningUploader )
    {
        emptyPictureBuf->setColorFormat( PIX_FMT_YUV420P );
        return true;
    }
#endif

    static const int MAX_PLANE_COUNT = 4;
    static const int Y_PLANE_INDEX = 0;
    static const int A_PLANE_INDEX = 3;

    const int planeCount = format == PIX_FMT_YUVA420P ? 4 : 3;

    unsigned int r_w[MAX_PLANE_COUNT];
    r_w[0] = width;
    r_w[1] = width / 2;
    r_w[2] = width / 2;
    r_w[3] = width; //alpha plane
    unsigned int h[MAX_PLANE_COUNT];
    h[0] = height;
    h[1] = height / 2;
    h[2] = height / 2;
    h[3] = height; //alpha plane

    switch( format )
    {
        case PIX_FMT_YUV444P:
            r_w[1] = r_w[2] = width;
        // fall through
        case PIX_FMT_YUV422P:
            h[1] = h[2] = height;
            break;
        default:
            break;
    }

    if( (format == PIX_FMT_YUV420P || format == PIX_FMT_YUV422P || format == PIX_FMT_YUV444P || format == PIX_FMT_YUVA420P) && usingShaderYuvToRgb() )
    {
        //using pixel shader for yuv->rgb conversion
        for( int i = 0; i < planeCount; ++i )
        {
            QnGlRendererTexture* texture = emptyPictureBuf->texture(i);
            texture->ensureInitialized( r_w[i], h[i], lineSizes[i], 1, GL_LUMINANCE, 1, /*-1*/ i == Y_PLANE_INDEX ? 0x10 : (i == A_PLANE_INDEX ? 0x00 : 0x80) );

#ifdef USE_PBO
#ifdef USE_SINGLE_PBO_PER_FRAME
            const unsigned int pboIndex = 0;
#else
            const unsigned int pboIndex = i;
#endif
            ensurePBOInitialized( emptyPictureBuf, pboIndex, lineSizes[i]*h[i] );
            d->glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, emptyPictureBuf->pboID(pboIndex) );
#ifdef DIRECT_COPY
            GLvoid* pboData = d->glMapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB );
            Q_ASSERT( pboData );
            memcpy( pboData, planes[i], lineSizes[i]*h[i] );
            Q_ASSERT( (size_t)planes[i] % 16 == 0 );
            Q_ASSERT( (lineSizes[i]*h[i]) % 16 == 0 );
            //memcpy_sse4_stream_store( (__m128i*)pboData, (__m128i*)planes[i], lineSizes[i]*h[i] );
            d->glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB );

            //glFlush();
            //glFinish();
#else
            d->glBufferSubData(
                GL_PIXEL_UNPACK_BUFFER_ARB,
                0,
                lineSizes[i]*h[i],
                planes[i] );
#endif
#endif

            NX_LOG( QString::fromLatin1("uploading to gl texture. id = %1, i = %2, lineSizes[i] = %3, r_w[i] = %4, "
                "qPower2Ceil(r_w[i],ROUND_COEFF) = %5, h[i] = %6, planes[i] = %7").
                arg(texture->id()).arg(i).arg(lineSizes[i]).arg(r_w[i]).arg(qPower2Ceil(r_w[i],ROUND_COEFF)).arg(h[i]).arg((size_t)planes[i]), cl_logDEBUG2 );
            glBindTexture( GL_TEXTURE_2D, texture->id() );
            DEBUG_CODE(glCheckError("glBindTexture"));

            const quint64 lineSizes_i = lineSizes[i];
            const quint64 r_w_i = r_w[i];
            glPixelStorei(GL_UNPACK_ROW_LENGTH, lineSizes_i);
            DEBUG_CODE(glCheckError("glPixelStorei"));
            Q_ASSERT( lineSizes_i >= qPower2Ceil(r_w_i,ROUND_COEFF) );
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            qPower2Ceil(r_w_i,ROUND_COEFF),
#ifdef PARTIAL_FRAME_UPLOAD
                            1,
#else
                            h[i],
#endif
                            GL_LUMINANCE, GL_UNSIGNED_BYTE,
#ifndef USE_PBO
                            planes[i]
#else
                            NULL
#endif
                            );
            DEBUG_CODE(glCheckError("glTexSubImage2D"));

            bitrateCalculator.bytesProcessed( qPower2Ceil(r_w[i],ROUND_COEFF)*h[i] );
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            DEBUG_CODE(glCheckError("glPixelStorei"));

#ifdef USE_PBO
            d->glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );
#ifdef USE_SINGLE_PBO_PER_FRAME
            //have to synchronize every time, since single PBO buffer used
            glFlush();
            glFinish();
#endif
#endif
        }

        glBindTexture( GL_TEXTURE_2D, 0 );

        emptyPictureBuf->setColorFormat( PIX_FMT_YUV420P );
    }
    else if( format == PIX_FMT_NV12 && usingShaderNV12ToRgb() )
    {
        for( int i = 0; i < 2; ++i )
        {
            QnGlRendererTexture* texture = emptyPictureBuf->texture(i);
            //void ensureInitialized(int width, int height, int stride, int pixelSize, GLint internalFormat, int internalFormatPixelSize, int fillValue) {
            if( i == 0 )    //Y-plane
                texture->ensureInitialized( width, height, lineSizes[i], 1, GL_LUMINANCE, 1, -1 );
            else
                texture->ensureInitialized( width / 2, height / 2, lineSizes[i], 2, GL_LUMINANCE_ALPHA, 2, -1 );
            glBindTexture( GL_TEXTURE_2D, texture->id() );
            const uchar* pixels = planes[i];
            glPixelStorei( GL_UNPACK_ROW_LENGTH, i == 0 ? lineSizes[0] : (lineSizes[1]/2) );
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            i == 0 ? qPower2Ceil(width,ROUND_COEFF) : width / 2,
                            i == 0 ? height : height / 2,
                            i == 0 ? GL_LUMINANCE : GL_LUMINANCE_ALPHA,
                            GL_UNSIGNED_BYTE, pixels );
            DEBUG_CODE(glCheckError("glTexSubImage2D"));
            glBindTexture( GL_TEXTURE_2D, 0 );
            bitrateCalculator.bytesProcessed( (i == 0 ? qPower2Ceil(width,ROUND_COEFF) : width / 2)*(i == 0 ? height : height / 2) );
            //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            //DEBUG_CODE(glCheckError("glPixelStorei"));
        }

        emptyPictureBuf->setColorFormat( PIX_FMT_NV12 );
    }
    else
    {
        QMutexLocker lk( &m_uploadMutex );

        //software conversion data to rgb
        QnGlRendererTexture* texture = emptyPictureBuf->texture(0);

        int bytesPerPixel = 1;
        if( !isYuvFormat(format) )
        {
            if( format == PIX_FMT_RGB24 || format == PIX_FMT_BGR24 )
                bytesPerPixel = 3;
            else
                bytesPerPixel = 4;
        }

        texture->ensureInitialized(r_w[0], h[0], lineSizes[0], bytesPerPixel, GL_RGBA, 4, 0);
        glBindTexture(GL_TEXTURE_2D, texture->id());

        uchar* pixels = planes[0];
        if (isYuvFormat(format))
        {
            int size = 4 * lineSizes[0] * h[0];
            if (m_yuv2rgbBufferLen < size)
            {
                m_yuv2rgbBufferLen = size;
                qFreeAligned(m_yuv2rgbBuffer);
                m_yuv2rgbBuffer = (uchar*)qMallocAligned(size, CL_MEDIA_ALIGNMENT);
            }
            pixels = m_yuv2rgbBuffer;
        }

        int lineInPixelsSize = lineSizes[0];
        switch (format)
        {
            case PIX_FMT_YUV420P:
                if (useSSE2())
                {
                    yuv420_argb32_sse2_intr(pixels, planes[0], planes[2], planes[1],
                        qPower2Ceil(r_w[0],ROUND_COEFF),
                        h[0],
                        4 * lineSizes[0],
                        lineSizes[0], lineSizes[1], opacity()*255);
                }
                else {
                    NX_LOG("CPU does not contain SSE2 module. Color space convert is not implemented", cl_logWARNING);
                }
                break;

            case PIX_FMT_YUV422P:
                if (useSSE2())
                {
                    yuv422_argb32_sse2_intr(pixels, planes[0], planes[2], planes[1],
                        qPower2Ceil(r_w[0],ROUND_COEFF),
                        h[0],
                        4 * lineSizes[0],
                        lineSizes[0], lineSizes[1], opacity()*255);
                }
                else {
                    NX_LOG("CPU does not contain SSE2 module. Color space convert is not implemented", cl_logWARNING);
                }
                break;

            case PIX_FMT_YUV444P:
                if (useSSE2())
                {
                    yuv444_argb32_sse2_intr(pixels, planes[0], planes[2], planes[1],
                        qPower2Ceil(r_w[0],ROUND_COEFF),
                        h[0],
                        4 * lineSizes[0],
                        lineSizes[0], lineSizes[1], opacity()*255);
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

        glPixelStorei(GL_UNPACK_ROW_LENGTH, lineInPixelsSize);
        DEBUG_CODE(glCheckError("glPixelStorei"));

        glTexSubImage2D(GL_TEXTURE_2D, 0,
            0, 0,
            qPower2Ceil(r_w[0],ROUND_COEFF),
            h[0],
            glRGBFormat(format), GL_UNSIGNED_BYTE, pixels);
        bitrateCalculator.bytesProcessed( qPower2Ceil(r_w[0],ROUND_COEFF)*h[0]*4 );
        DEBUG_CODE(glCheckError("glTexSubImage2D"));

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        DEBUG_CODE(glCheckError("glPixelStorei"));

        glBindTexture( GL_TEXTURE_2D, 0 );

        emptyPictureBuf->setColorFormat( PIX_FMT_RGBA );

        //TODO: #ak free memory immediately for still images
    }

    //TODO/IMPL should place fence here and in getUploadedPicture should take only that picture, whose m_glFence is signaled
#ifdef SYNC_UPLOADING_WITH_GLFENCE
    emptyPictureBuf->m_glFence.placeFence();
#else
#ifdef UPLOAD_SYSMEM_FRAMES_IN_GUI_THREAD
    if( m_hardwareDecoderUsed )
#endif
    {
        glFlush();
        glFinish();
    }
#endif

    return true;
}

#ifdef GL_COPY_AGGREGATION
bool DecodedPictureToOpenGLUploader::uploadDataToGlWithAggregation(
    DecodedPictureToOpenGLUploader::UploadedPicture* const emptyPictureBuf,
    const PixelFormat format,
    const unsigned int width,
    const unsigned int height,
    uint8_t* planes[],
    int lineSizes[],
    bool /*isVideoMemory*/ )
{
    //taking aggregation surface having enough free space
    const QSharedPointer<AggregationSurfaceRect>& surfaceRect = AggregationSurfacePool::instance()->takeSurfaceRect(
        m_uploadThread->glContext(),
        format,
        QSize(width, height) );
    if( !surfaceRect )
        return false;

    //copying data to aggregation surface
#ifndef DISABLE_FRAME_UPLOAD
#ifdef SINGLE_STREAM_UPLOAD
    if( this == runningUploader )
#endif  //SINGLE_STREAM_UPLOAD
    surfaceRect->uploadData( planes, lineSizes, 3 ); //TODO/IMPL exact plane count
#endif  //DISABLE_FRAME_UPLOAD

    //filling emptyPictureBuf
    emptyPictureBuf->setColorFormat( format );
    emptyPictureBuf->setAggregationSurfaceRect( surfaceRect );

#ifndef UPLOAD_TO_GL_IN_GUI_THREAD
    if( GetTickCount() - surfaceRect->surface()->prevGLUploadClock > 300 )
    {
        surfaceRect->ensureUploadedToOGL( opacity() );
        surfaceRect->surface()->prevGLUploadClock = GetTickCount();
    }
#endif

    return true;
}
#endif

bool DecodedPictureToOpenGLUploader::usingShaderYuvToRgb() const
{
    return (d->features() & QnGlFunctions::ArbPrograms)
        && (d->features() & QnGlFunctions::OpenGL1_3)
        && !(d->features() & QnGlFunctions::ShadersBroken)
        && m_yv12SharedUsed
        && !m_forceSoftYUV;
}

bool DecodedPictureToOpenGLUploader::usingShaderNV12ToRgb() const
{
    return (d->features() & QnGlFunctions::ArbPrograms)
        && (d->features() & QnGlFunctions::OpenGL1_3)
        && !(d->features() & QnGlFunctions::ShadersBroken)
        && m_nv12SharedUsed
        && !m_forceSoftYUV;
}

void DecodedPictureToOpenGLUploader::releaseDecodedPicturePool( std::deque<UploadedPicture*>* const pool )
{
    foreach( UploadedPicture* pic, *pool )
    {
        for( size_t i = 0; i < pic->m_pbo.size(); ++i )
        {
            if( pic->m_pbo[i].id == std::numeric_limits<GLuint>::max() )
                continue;
            d->glDeleteBuffers( 1, &pic->m_pbo[i].id );
            pic->m_pbo[i].id = std::numeric_limits<GLuint>::max();
        }
        delete pic;
    }
    pool->clear();
}

unsigned int DecodedPictureToOpenGLUploader::nextPicSequenceValue()
{
    unsigned int seq = m_previousPicSequence++;
    if( m_previousPicSequence == 0 )    //0 is a reserved value
        ++m_previousPicSequence;
    return seq;
}

void DecodedPictureToOpenGLUploader::ensurePBOInitialized(
    DecodedPictureToOpenGLUploader::UploadedPicture* const picBuf,
    unsigned int pboIndex,
    size_t sizeInBytes )
{
    if( picBuf->m_pbo.size() <= pboIndex )
        picBuf->m_pbo.resize( pboIndex+1 );

    if( picBuf->m_pbo[pboIndex].id == std::numeric_limits<GLuint>::max() )
    {
        d->glGenBuffers( 1, &picBuf->m_pbo[pboIndex].id );
        DEBUG_CODE(glCheckError("glGenBuffers"));
        picBuf->m_pbo[pboIndex].sizeBytes = 0;
    }

    if( picBuf->m_pbo[pboIndex].sizeBytes < sizeInBytes )
    {
        d->glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, picBuf->m_pbo[pboIndex].id );
        DEBUG_CODE(glCheckError("glBindBuffer"));
//GL_STREAM_DRAW_ARB 
//GL_STREAM_READ_ARB 
//GL_STREAM_COPY_ARB 
//GL_STATIC_DRAW_ARB 
//GL_STATIC_READ_ARB 
//GL_STATIC_COPY_ARB 
//GL_DYNAMIC_DRAW_ARB
//GL_DYNAMIC_READ_ARB
//GL_DYNAMIC_COPY_ARB
        d->glBufferData( GL_PIXEL_UNPACK_BUFFER_ARB, sizeInBytes, NULL, GL_STREAM_DRAW_ARB/*GL_STATIC_DRAW_ARB*/ );
        DEBUG_CODE(glCheckError("glBufferData"));
        d->glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );
        DEBUG_CODE(glCheckError("glBindBuffer"));
        picBuf->m_pbo[pboIndex].sizeBytes = sizeInBytes;
    }
}

void DecodedPictureToOpenGLUploader::releasePictureBuffers()
{
    QMutexLocker lk( &m_mutex );
    releasePictureBuffersNonSafe();
    m_cond.wakeAll();
}

void DecodedPictureToOpenGLUploader::releasePictureBuffersNonSafe()
{
    releaseDecodedPicturePool( &m_emptyBuffers );
    releaseDecodedPicturePool( &m_renderedPictures );
    releaseDecodedPicturePool( &m_picturesWaitingRendering );
    releaseDecodedPicturePool( &m_picturesBeingRendered );
}

static void yv12ToRgba(
    int width, int height,
    quint8* yp, size_t y_stride,
    quint8* up, size_t u_stride,
    quint8* vp, size_t v_stride,
    quint8* const rgbaBuf )
{
    static const int PIXEL_SIZE = 4;

    for( int y = 0; y < height; ++y )
    {
        for( int x = 0; x < width; ++x )
        {
            const int Y = *((quint8*)yp + y*y_stride + x);
            const int U = *((quint8*)up + (y>>1)*u_stride + (x>>1));
            const int V = *((quint8*)vp + (y>>1)*v_stride + (x>>1));

            int r = 1.164*(Y-16) + 1.596*(V-128);
            int g = 1.164*(Y-16) - 0.813*(V-128) - 0.391*(U-128);
            int b = 1.164*(Y-16)                 + 2.018*(U-128);

            r = std::min<>( 255, std::max<>( r, 0 ) );
            g = std::min<>( 255, std::max<>( g, 0 ) );
            b = std::min<>( 255, std::max<>( b, 0 ) );

            (rgbaBuf + (y*width + x)*PIXEL_SIZE)[0] = r;
            (rgbaBuf + (y*width + x)*PIXEL_SIZE)[1] = g;
            (rgbaBuf + (y*width + x)*PIXEL_SIZE)[2] = b;
            (rgbaBuf + (y*width + x)*PIXEL_SIZE)[3] = 255; //a
        }
    }
}

void DecodedPictureToOpenGLUploader::savePicToFile( AVFrame* const pic, int pts )
{
    if( !m_rgbaBuf )
        m_rgbaBuf = new uint8_t[pic->width*pic->height*4];

    yv12ToRgba(
        pic->width, pic->height,
        pic->data[0], pic->linesize[0],
        pic->data[1], pic->linesize[1],
        pic->data[2], pic->linesize[2],
        m_rgbaBuf );

	QImage img(
		m_rgbaBuf,
		pic->width,
		pic->height,
        QImage::Format_ARGB32_Premultiplied );	//QImage::Format_ARGB4444_Premultiplied );
    const QString& fileName = QString::fromAscii("C:\\temp\\%1_%2.bmp").arg(m_fileNumber++, 3, 10, QLatin1Char('0')).arg(pts);
    img.save(fileName, "bmp");
    /*if( !img.save( fileName, "bmp" ) )
        int x = 0;*/
}
