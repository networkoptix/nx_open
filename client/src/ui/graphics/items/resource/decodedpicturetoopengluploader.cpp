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

#include <utils/yuvconvert.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/graphics/opengl/gl_context_data.h>

//#define RENDERER_SUPPORTS_NV12
//#define USE_PBO


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

        const DWORD currentTick = GetTickCount();
        if( currentTick - m_startCalcTick > 5000 )
        {
            NX_LOG( QString::fromAscii("In previous %1 ms to video mem moved %2 bytes. Transfer rate %3 byte/second").
                arg(currentTick - m_startCalcTick).arg(m_bytes).arg(m_bytes * 1000 / (currentTick - m_startCalcTick)), cl_logDEBUG1 );
            m_startCalcTick = currentTick;
            m_bytes = 0;
        }
        m_bytes += byteCount;

        m_mtx.unlock();
    }

private:
    DWORD m_startCalcTick;
    quint64 m_bytes;
    QMutex m_mtx;
};

static BitrateCalculator bitrateCalculator;


//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploaderContextPool (OpenGL context pool)
//////////////////////////////////////////////////////////
Q_GLOBAL_STATIC(DecodedPictureToOpenGLUploaderContextPool, qn_decodedPictureToOpenGLUploaderContextPool);

DecodedPictureToOpenGLUploaderContextPool::DecodedPictureToOpenGLUploaderContextPool()
:
    m_paintWindowId( NULL ),
    m_optimalGLContextPoolSize( 0 )
{
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    memset( &sysInfo, 0, sizeof(sysInfo) );
    GetSystemInfo( &sysInfo );
    m_optimalGLContextPoolSize = sysInfo.dwNumberOfProcessors;
#else
    //TODO/IMPL calculate optimal pool size
    m_optimalGLContextPoolSize = 4;
#endif
}

DecodedPictureToOpenGLUploaderContextPool::~DecodedPictureToOpenGLUploaderContextPool()
{
    for( std::map<GLContext::SYS_GL_CTX_HANDLE, SafePool<GLContext*, QSharedPointer<GLContext> >* >::iterator
        it = m_auxiliaryGLContextPool.begin();
        it != m_auxiliaryGLContextPool.end();
         )
    {
        delete it->second;
        m_auxiliaryGLContextPool.erase( it++ );
    }
}

void DecodedPictureToOpenGLUploaderContextPool::setPaintWindowHandle( WId paintWindowId )
{
    m_paintWindowId = paintWindowId;
}

WId DecodedPictureToOpenGLUploaderContextPool::paintWindowHandle() const
{
    return m_paintWindowId;
}

bool DecodedPictureToOpenGLUploaderContextPool::ensureThereAreContextsSharedWith(
    GLContext::SYS_GL_CTX_HANDLE parentContextID,
    WId winID,
    int poolSizeIncrement )
{
    SafePool<GLContext*, QSharedPointer<GLContext> >* pool = getPoolOfContextsSharedWith( parentContextID );
    if( pool->empty() )
    {
        if( poolSizeIncrement < 0 )
            poolSizeIncrement = m_optimalGLContextPoolSize;
        for( int i = 0; i < poolSizeIncrement; ++i )
        {
            //creating context
            QSharedPointer<GLContext> auxiliaryGLContext( new GLContext( winID, parentContextID ) );
            if( !auxiliaryGLContext->isValid() )
                break;
            pool->insert( std::make_pair( auxiliaryGLContext.data(), auxiliaryGLContext ) );
        }
    }

    return !pool->empty();
}

SafePool<GLContext*, QSharedPointer<GLContext> >* DecodedPictureToOpenGLUploaderContextPool::getPoolOfContextsSharedWith( GLContext::SYS_GL_CTX_HANDLE parentContext ) const
{
    QMutexLocker lk( &m_mutex );

    SafePool<GLContext*, QSharedPointer<GLContext> >*& pool = m_auxiliaryGLContextPool[parentContext];
    if( !pool )
        pool = new SafePool<GLContext*, QSharedPointer<GLContext> >();
    return pool;
}

DecodedPictureToOpenGLUploaderContextPool* DecodedPictureToOpenGLUploaderContextPool::instance()
{
    return qn_decodedPictureToOpenGLUploaderContextPool();
}

// -------------------------------------------------------------------------- //
// DecodedPictureToOpenGLUploaderPrivate
// -------------------------------------------------------------------------- //
class DecodedPictureToOpenGLUploaderPrivate
:
    public QnGlFunctions
{
    Q_DECLARE_TR_FUNCTIONS(DecodedPictureToOpenGLUploaderPrivate);

public:
    static int getMaxTextureSize() { return maxTextureSize; }

    DecodedPictureToOpenGLUploaderPrivate(const QGLContext *context):
        QnGlFunctions(context),
        supportsNonPower2Textures(false)
    {
        QByteArray extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        QByteArray version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        QByteArray renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

        /* Maximal texture size. */
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        NX_LOG(QString(QLatin1String("OpenGL max texture size: %1.")).arg(maxTextureSize), cl_logINFO);

        /* Clamp constant. */
        clampConstant = GL_CLAMP;
        if (extensions.contains("GL_EXT_texture_edge_clamp") || extensions.contains("GL_SGIS_texture_edge_clamp") || version >= QByteArray("1.2.0"))
            clampConstant = GL_CLAMP_TO_EDGE;

        /* Check for non-power of 2 textures. */
        supportsNonPower2Textures = extensions.contains("GL_ARB_texture_non_power_of_two");
    }

    uchar *filler(uchar value, int size)
    {
        QMutexLocker locker(&fillerMutex);

        QVector<uchar> &filler = fillers[value];

        if(filler.size() < size) {
            filler.resize(size);
            filler.fill(value);
        }

        return &filler[0];
    }

public:
    GLint clampConstant;
    bool supportsNonPower2Textures;
    static int maxTextureSize;

private:
    QMutex fillerMutex;
    QVector<uchar> fillers[256];
};

int DecodedPictureToOpenGLUploaderPrivate::maxTextureSize = 0;

typedef QnGlContextData<
    DecodedPictureToOpenGLUploaderPrivate,
    QnGlContextDataForwardingFactory<DecodedPictureToOpenGLUploaderPrivate>
> DecodedPictureToOpenGLUploaderPrivateStorage;
Q_GLOBAL_STATIC(DecodedPictureToOpenGLUploaderPrivateStorage, qn_decodedPictureToOpenGLUploaderPrivateStorage);


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
        m_id(-1),
        m_fillValue(-1),
        m_renderer(renderer)
    {}

    ~QnGlRendererTexture() {
        //NOTE we do not delete texture here because it belongs to auxiliary gl context which will be removed when these textures are not needed anymore
        glDeleteTextures(1, &m_id);
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
        assert(m_renderer.data() != NULL);

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
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, textureSize.width(), textureSize.height(), 0, internalFormat, GL_UNSIGNED_BYTE, 0);
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
            int fillSize = qMax(textureSize.height(), textureSize.width()) * ROUND_COEFF * internalFormatPixelSize * 16;
            uchar *filler = m_renderer->filler(fillValue, fillSize);

            if (roundedWidth < textureSize.width()) {
                glBindTexture(GL_TEXTURE_2D, m_id);
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
                glCheckError("glTexSubImage2D");
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
                glCheckError("glTexSubImage2D");
            }
        }
    }

    void ensureAllocated() {
        if(m_allocated)
            return;

        glGenTextures(1, &m_id);
        glCheckError("glGenTextures");

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
SafePool<GLContext*, QSharedPointer<GLContext> >* DecodedPictureToOpenGLUploader::UploadedPicture::glContextPool() const
{
    return m_glContextPool;
}

GLContext* DecodedPictureToOpenGLUploader::UploadedPicture::glContext() const
{
    return m_glContext;
}

PixelFormat DecodedPictureToOpenGLUploader::UploadedPicture::colorFormat() const
{
    return m_colorFormat;
}

int DecodedPictureToOpenGLUploader::UploadedPicture::width() const
{
    return m_width;
}

int DecodedPictureToOpenGLUploader::UploadedPicture::height() const
{
    return m_height;
}

const QVector2D& DecodedPictureToOpenGLUploader::UploadedPicture::texCoords() const
{
    //TODO/IMPL
    return m_textures[0]->texCoords();
}

const std::vector<GLuint>& DecodedPictureToOpenGLUploader::UploadedPicture::glTextures() const
{
    m_picTextures.resize( TEXTURE_COUNT );
    for( size_t i = 0; i < TEXTURE_COUNT; ++i )
        m_picTextures[i] = m_textures[i]->id();
    return m_picTextures;
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


//const int DecodedPictureToOpenGLUploader::UploadedPicture::TEXTURE_COUNT;

DecodedPictureToOpenGLUploader::UploadedPicture::UploadedPicture(
    DecodedPictureToOpenGLUploader* const uploader,
    SafePool<GLContext*, QSharedPointer<GLContext> >* const _glContextPool,
    GLContext* const _glContext )
:
    m_glContextPool( _glContextPool ),
    m_glContext( _glContext ),
    m_colorFormat( PIX_FMT_NONE ),
    m_width( 0 ),
    m_height( 0 ),
    m_sequence( 0 ),
    m_pts( 0 )
{
    //TODO/IMPL allocate textures when needed, because not every format require 3 planes
    for( int i = 0; i < TEXTURE_COUNT; ++i )
        m_textures[i].reset(new QnGlRendererTexture(uploader->d));
}

DecodedPictureToOpenGLUploader::UploadedPicture::~UploadedPicture()
{
    //TODO/IMPL
}

QnGlRendererTexture* DecodedPictureToOpenGLUploader::UploadedPicture::texture( int index ) const
{
    return m_textures[index].data();
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
// AsyncUploader
//////////////////////////////////////////////////////////

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

inline void copyImageRectStreamLoad(
    quint8* dstMem,
    unsigned int dstWidth,
    unsigned int dstHeight,
    unsigned int dstPitch,
    const quint8* srcMem,
    unsigned int srcPitch )
{
    static const unsigned int PIXEL_SIZE = 1;
    for( int y = 0; y < dstHeight; ++y )
        memcpy_stream_load( (__m128i*)(dstMem+y*dstPitch), (__m128i*)(srcMem+y*srcPitch), dstWidth*PIXEL_SIZE );
}

/*!
    Deinterleaves NV12 UV-plane rect (\a srcWidth, \a srcHeight) starting at \a srcMem to \a dstUPlane and \a dstVPlane
*/
inline void streamLoadAndDeinterleaveNV12UVPlaneEx(
    const quint8* srcMem,
    unsigned int srcWidth,
    unsigned int srcHeight,
    size_t srcPitch,
    quint8* dstUPlane,
    quint8* dstVPlane,
    size_t dstPitch )
{
    //Q_ASSERT( dstPitch % 64 == 0 );

    for( int y = 0; y < srcHeight / 2; ++y )
    {
        streamLoadAndDeinterleaveNV12UVPlane(
            (__m128i*)(srcMem + y*srcPitch),
            dstPitch * 2,   //not using srcPitch here because we deinterleave only one not full line
            //srcPitch,
            (__m128i*)(dstUPlane + y*dstPitch),
            (__m128i*)(dstVPlane + y*dstPitch) );
    }
}

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
class AsyncUploader
:
    public QRunnable
{
public:
    AsyncUploader(
        DecodedPictureToOpenGLUploader::UploadedPicture* pictureBuf,
        const QSharedPointer<QnAbstractPictureDataRef>& picDataRef,
        DecodedPictureToOpenGLUploader* uploader )
    :
        m_pictureBuf( pictureBuf ),
        m_picDataRef( picDataRef ),
        m_uploader( uploader ),
        m_yv12Buffer( NULL ),
        m_yv12BufferCapacity( 0 )
    {
        setAutoDelete( true );
        memset( m_planes, 0, sizeof(m_planes) );
        memset( m_lineSizes, 0, sizeof(m_lineSizes) );
    }

    AsyncUploader::~AsyncUploader()
    {
        qFreeAligned( m_yv12Buffer );
    }

    //TODO/IMPL interrupting upload

    virtual void run()
    {
#ifdef _WIN32
        //checking, if m_picDataRef ref has not been marked for released
        ScopedAtomicLock picUsageCounterLock( &m_picDataRef->syncCtx()->usageCounter );
        if( !m_picDataRef->isValid() ) //m_picDataRef->syncCtx()->sequence != m_sequence
        {
            NX_LOG( QString::fromAscii("AsyncUploader. Frame (pts %1, 0x%2) data ref has been invalidated. Releasing...").
                arg(m_pictureBuf->pts()).arg((size_t)m_picDataRef->syncCtx(), 0, 16), cl_logDEBUG1 );
            m_uploader->pictureDataUploadFailed( m_picDataRef, m_pictureBuf );
            return;
        }

        Q_ASSERT( m_picDataRef->type() == QnAbstractPictureDataRef::pstD3DSurface );
        IDirect3DSurface9* const surf = static_cast<D3DPictureData*>(m_picDataRef.data())->getSurface();
        D3DSURFACE_DESC surfDesc;
        memset( &surfDesc, 0, sizeof(surfDesc) );
        HRESULT res = surf->GetDesc( &surfDesc );
        if( res != D3D_OK )
        {
            NX_LOG( QString::fromAscii("AsyncUploader. Failed to get dxva surface info (%1). Ignoring decoded picture...").arg(res), cl_logERROR );
            m_uploader->pictureDataUploadFailed( m_picDataRef, m_pictureBuf );
            return;
        }

        if( surfDesc.Format != (D3DFORMAT)MAKEFOURCC('N','V','1','2') )
        {
            NX_LOG( QString::fromAscii("AsyncUploader. Dxva surface format %1 while only NV12 (%2) is supported. Ignoring decoded picture...").
                arg(surfDesc.Format).arg(MAKEFOURCC('N','V','1','2')), cl_logERROR );
            m_uploader->pictureDataUploadFailed( m_picDataRef, m_pictureBuf );
            return;
        }

        D3DLOCKED_RECT lockedRect; 
        memset( &lockedRect, 0, sizeof(lockedRect) );
        RECT rectToLock;
        memset( &rectToLock, 0, sizeof(rectToLock) );
        rectToLock.left = m_picDataRef->cropRect().left();
        rectToLock.top = m_picDataRef->cropRect().top();
        rectToLock.right = m_picDataRef->cropRect().right() + 1;
        rectToLock.bottom = m_picDataRef->cropRect().bottom() + 1;
        res = surf->LockRect( &lockedRect, NULL /*&rectToLock*/, 0/*D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY*/ );
        if( res != D3D_OK )
        {
            NX_LOG( QString::fromAscii("AsyncUploader. Failed to map dxva surface (%1). Ignoring decoded picture...").
                arg(QString::fromWCharArray(DXGetErrorDescription(res))), cl_logERROR );
            surf->UnlockRect();
            m_uploader->pictureDataUploadFailed( m_picDataRef, m_pictureBuf );
            return;
        }

        //using crop rect of frame

        const unsigned int targetPitch = minPow2( m_picDataRef->cropRect().width() );
        const unsigned int targetHeight = m_picDataRef->cropRect().height();

        //converting to YV12
        if( m_yv12BufferCapacity < targetHeight * targetPitch * 3 / 2 )
        {
            m_yv12BufferCapacity = targetHeight * targetPitch * 3 / 2;
            qFreeAligned( m_yv12Buffer );
            m_yv12Buffer = (uint8_t*)qMallocAligned( m_yv12BufferCapacity, sizeof(__m128i) );
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
        //Y-plane
        copyImageRectStreamLoad(
            (quint8*)m_planes[0],
            m_picDataRef->cropRect().width(),
            m_picDataRef->cropRect().height(),
            targetPitch,
            (const quint8*)lockedRect.pBits,
            lockedRect.Pitch );
#ifdef RENDERER_SUPPORTS_NV12
        copyImageRectStreamLoad(
            (quint8*)m_planes[1],
            m_picDataRef->cropRect().width(),       //X resolution is 1/2, but every pixel has 2 bytes
            m_picDataRef->cropRect().height() / 2,  //Y resolution is 1/2, so number of lines is 2 times less
            targetPitch,
            (const quint8*)lockedRect.pBits + lockedRect.Pitch * surfDesc.Height,
            lockedRect.Pitch );
#else
        streamLoadAndDeinterleaveNV12UVPlaneEx(
            (quint8*)lockedRect.pBits + lockedRect.Pitch * surfDesc.Height,
            m_picDataRef->cropRect().width(),
            m_picDataRef->cropRect().height(),
            lockedRect.Pitch,
            (quint8*)m_planes[1],
            (quint8*)m_planes[2],
            targetPitch / 2 );
#endif
        {
            SafePool<GLContext*, QSharedPointer<GLContext> >::ScopedReadLock glCtxLock(
                m_pictureBuf->glContextPool(),
                m_pictureBuf->glContextPool()->lock( m_pictureBuf->glContext() ) );
            Q_ASSERT( glCtxLock.isValid() );
            //QMutexLocker lk( &m_uploader->m_uploadMutex );
            m_uploader->uploadDataToGl(
                m_pictureBuf->glContext(),
                m_pictureBuf,
#ifdef RENDERER_SUPPORTS_NV12
                PIX_FMT_NV12,
#else
                PIX_FMT_YUV420P,
#endif
                m_picDataRef->cropRect().width(),
                m_picDataRef->cropRect().height(),
                m_planes,
                m_lineSizes,
                true );
        }
        //memcpy_stream_load(
        //    (__m128i*)m_planes[0],
        //    (__m128i*)lockedRect.pBits,
        //    lockedRect.Pitch * surfDesc.Height );
        //streamLoadAndDeinterleaveNV12UVPlane(
        //    (__m128i*)((uint8_t*)lockedRect.pBits + lockedRect.Pitch * surfDesc.Height),
        //    lockedRect.Pitch * surfDesc.Height / 2,
        //    (__m128i*)m_planes[1],
        //    (__m128i*)m_planes[2] );
        //{
        //    QMutexLocker lk( &m_uploader->m_uploadMutex );
        //    m_uploader->uploadDataToGl( m_uploader->m_glContext, m_pictureBuf, PIX_FMT_YUV420P, surfDesc.Width, surfDesc.Height, m_planes, m_lineSizes, true );
        //}
 
        //assuming surface format is always NV12
        //m_planes[0] = (uint8_t*)lockedRect.pBits;
        //m_lineSizes[0] = lockedRect.Pitch;
        //m_planes[1] = (uint8_t*)lockedRect.pBits + surfDesc.Height * lockedRect.Pitch;
        //m_lineSizes[1] = lockedRect.Pitch;
        //{
        //    QMutexLocker lk( &m_uploader->m_uploadMutex );
        //    m_uploader->uploadDataToGl( m_uploader->m_glContext, m_pictureBuf, PIX_FMT_NV12, surfDesc.Width, surfDesc.Height, m_planes, m_lineSizes, true );
        //}

        surf->UnlockRect();
#endif
        m_uploader->pictureDataUploadSucceeded( m_picDataRef, m_pictureBuf );
    }

#ifdef USE_PBO
    virtual void run()
    {
        //TODO/IMPL prepare target planes
        readNV12Picture( m_picDataRef, planes, lineSizes, true, isNV12 );
    }

    //!Copies nv12 data from \a picDataRef to \a planes with line sizes from \a linesizes
    /*!
        \param planes Array of allocated planes with enough size
        \param isTargetMemoryIsVideoMemory If true, sse stream operations are used to upload data to \a planes
        \param convertToYV12 If true, data is converted to yv12
    */
    bool readNV12Picture(
        const QSharedPointer<QnAbstractPictureDataRef>& picDataRef,
        uint8_t* planes[],
        int linesizes[],
        bool isTargetMemoryIsVideoMemory,
        bool convertToYV12 )
    {
    }
#endif

private:
    DecodedPictureToOpenGLUploader::UploadedPicture* m_pictureBuf;
    QSharedPointer<QnAbstractPictureDataRef> m_picDataRef;
    DecodedPictureToOpenGLUploader* m_uploader;
    uint8_t* m_planes[3];
    int m_lineSizes[3];
    uint8_t* m_yv12Buffer;
    size_t m_yv12BufferCapacity;
};

static QThreadPool glTextureUploadThreadPool;
class GLTextureUploadThreadPoolInitializer
{
public:
    GLTextureUploadThreadPoolInitializer()
    {
#ifdef _WIN32
        SYSTEM_INFO sysInfo;
        memset( &sysInfo, 0, sizeof(sysInfo) );
        GetSystemInfo( &sysInfo );
        glTextureUploadThreadPool.setMaxThreadCount( sysInfo.dwNumberOfProcessors );
#else
        //TODO/IMPL calculate optimal thread count
        glTextureUploadThreadPool.setMaxThreadCount( 2 );
#endif
    }
};

static GLTextureUploadThreadPoolInitializer gTextureUploadThreadPoolInitializer;

//////////////////////////////////////////////////////////
// GLTextureUploaderThread
//////////////////////////////////////////////////////////
/*
template<class T>
class SafeQueue
{
public:
    void push( const T& val )
    {
        //TODO/IMPL
    }

    T pop()
    {
        //TODO/IMPL
        return T();
    }
};

class DecodedPictureUploadTask
{
public:
    DecodedPictureToOpenGLUploader::UploadedPicture* pictureBuf;
    const QSharedPointer<QnAbstractPictureDataRef> picDataRef;

    DecodedPictureUploadTask(
        DecodedPictureToOpenGLUploader::UploadedPicture* _pictureBuf,
        const QSharedPointer<QnAbstractPictureDataRef>& _picDataRef )
    :
        pictureBuf( _pictureBuf ),
        picDataRef( _picDataRef )
    {
    }
};

class GLTextureUploaderThread
:
    public QThread
{
public:
    GLTextureUploaderThread( SafeQueue<DecodedPictureUploadTask>* const taskQueue )
    :
        m_taskQueue( taskQueue ),
        m_terminated( false )
    {
    }

    virtual ~GLTextureUploaderThread()
    {
        terminate();
        wait();
    }

    void terminate()
    {
        m_terminated = true;
    }

protected:
    virtual void run() override
    {
        while( !m_terminated )
        {
            DecodedPictureUploadTask task = m_taskQueue->pop();
            if( task.locked )
            {
                task = m_taskQueue->pop();
                m_taskQueue->push_front( task );
            }
            
        }
        //TODO/IMPL
    }

private:
    SafeQueue<DecodedPictureUploadTask>* const m_taskQueue;
    bool m_terminated;
};

class GLTextureUploaderThreadPool
{
public:
    GLTextureUploaderThreadPool()
    :
        m_optimalThreadCount( 2 )
    {
        //TODO/IMPL calculating optimal thread count
    }

    virtual ~GLTextureUploaderThreadPool()
    {
        std::for_each( m_threads.begin(), m_threads.end(), std::mem_fun(&GLTextureUploaderThread::terminate) );
        std::for_each( m_threads.begin(), m_threads.end(), std::bind2nd( std::mem_fun1(&GLTextureUploaderThread::wait), ULONG_MAX ) );
    }

    void addTask( const DecodedPictureUploadTask& task )
    {
        if( m_threads.empty() )
        {
            QMutexLocker lk( &m_mutex );
            if( m_threads.empty() )
            {
                //creating threads
                for( int i = 0; i < m_optimalThreadCount; ++i )
                    m_threads.push_back( new GLTextureUploaderThread(&m_taskQueue) );
            }
        }

        m_taskQueue.push( task );
    }

private:
    std::vector<GLTextureUploaderThread*> m_threads;
    SafeQueue<DecodedPictureUploadTask> m_taskQueue;
    int m_optimalThreadCount;
    QMutex m_mutex;
};

static GLTextureUploaderThreadPool glTextureUploaderThreadPool;
*/

//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploader
//////////////////////////////////////////////////////////

//TODO/IMPL minimize blocking in \a DecodedPictureToOpenGLUploader::uploadDataToGl method: 
    //before calling this method we can check that it would not block and take another task if it would

//TODO/IMPL interrupting surface upload

//TODO/IMPL use PBO

//TODO/IMPL use nv12->rgba shader

//we need second frame in case of using async upload to ensure renderer always gets something to draw
static const size_t MIN_GL_PIC_BUF_COUNT = 2;

DecodedPictureToOpenGLUploader::DecodedPictureToOpenGLUploader(
    const QGLContext* const mainContext,
    GLContext::SYS_GL_CTX_HANDLE mainContextHandle,
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
    m_nv12SharedUsed( false )
{
    //DecodedPictureToOpenGLUploaderPrivateStorage* storage = qn_decodedPictureToOpenGLUploaderPrivateStorage();
    //if( storage )
    //    d = qn_decodedPictureToOpenGLUploaderPrivateStorage()->get( glContext.data() );
    //if( d.isNull() )
    //    d = QSharedPointer<DecodedPictureToOpenGLUploaderPrivate>( new DecodedPictureToOpenGLUploaderPrivate(NULL) );
    SafePool<GLContext*, QSharedPointer<GLContext> >* pool = DecodedPictureToOpenGLUploaderContextPool::instance()->getPoolOfContextsSharedWith( mainContextHandle );
    Q_ASSERT( !pool->empty() );

    const std::vector<GLContext*>& allGLContexts = pool->keys();
    for( size_t i = 0; i < asyncDepth+MIN_GL_PIC_BUF_COUNT; ++i )
        m_emptyBuffers.push_back( new UploadedPicture(
            this,
            pool,
            allGLContexts[rand()%pool->size()] ) );    //binding frame buffer to random gl context
}

DecodedPictureToOpenGLUploader::~DecodedPictureToOpenGLUploader()
{
    QMutexLocker lk( &m_mutex );

    const QGLContext* glContext = QGLContext::currentContext();

    releaseDecodedPicturePool( &m_emptyBuffers );
    releaseDecodedPicturePool( &m_renderedPictures );
    releaseDecodedPicturePool( &m_picturesWaitingRendering );
    releaseDecodedPicturePool( &m_picturesBeingRendered );

    if( glContext )
        const_cast<QGLContext*>(glContext)->makeCurrent();
}

void DecodedPictureToOpenGLUploader::uploadDecodedPicture( const QSharedPointer<CLVideoDecoderOutput>& decodedPicture )
{
    NX_LOG( QString::fromAscii( "Uploading decoded picture to gl textures. pts %1" ).arg(decodedPicture->pkt_dts), cl_logDEBUG2 );

    const bool useAsyncUpload = decodedPicture->picData.data() && (decodedPicture->picData->type() == QnAbstractPictureDataRef::pstD3DSurface);
    m_format = decodedPicture->format;
    UploadedPicture* emptyPictureBuf = NULL;
    {
        QMutexLocker lk( &m_mutex );
        //searching for a non-used picture buffer
        for( ;; )
        {
            if( !m_emptyBuffers.empty() )
            {
                emptyPictureBuf = m_emptyBuffers.front();
                m_emptyBuffers.pop_front();
                NX_LOG( QString::fromAscii( "Found empty buffer" ), cl_logDEBUG2 );
            }
            else if( (!useAsyncUpload && !m_renderedPictures.empty())
                  || (useAsyncUpload && (m_renderedPictures.size() > (m_picturesWaitingRendering.empty() ? 1 : 0))) )  //reserving one uploaded picture (preferring picture 
                                                                                                                       //which has not been shown yet) so that renderer always 
                                                                                                                       //gets something to draw...
            {
                //selecting oldest rendered picture
                emptyPictureBuf = m_renderedPictures.front();
                m_renderedPictures.pop_front();
                NX_LOG( QString::fromAscii( "Taking rendered picture (pts %1) buffer for upload. (%2, %3)" ).
                    arg(emptyPictureBuf->pts()).arg(m_renderedPictures.size()).arg(m_picturesWaitingRendering.size()), cl_logDEBUG2 );
            }
            else if( (!useAsyncUpload && !m_picturesWaitingRendering.empty())
                  || (useAsyncUpload && (m_picturesWaitingRendering.size() > (m_renderedPictures.empty() ? 1 : 0))) )
            {
                //looks like rendering does not catch up with decoding. Ignoring oldest decoded frame...
                emptyPictureBuf = m_picturesWaitingRendering.front();
                m_picturesWaitingRendering.pop_front();
                NX_LOG( QString::fromAscii( "Ignoring decoded frame with pts %1. Uploading does not catch up with decoding. (%2, %3)..." ).
                    arg(emptyPictureBuf->pts()).arg(m_renderedPictures.size()).arg(m_picturesWaitingRendering.size()), cl_logINFO );
            }
            else
            {
                if( useAsyncUpload )
                {
                    //ignore decoded picture so that not to stop decoder
                    NX_LOG( QString::fromAscii( "Ignoring decoded frame with pts %1. Uploading does not catch up with decoding..." ).arg(decodedPicture->pkt_dts), cl_logINFO );
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
    emptyPictureBuf->m_sequence = m_previousPicSequence++;
    if( m_previousPicSequence == 0 )    //0 is a reserved value
        ++m_previousPicSequence;
    emptyPictureBuf->m_pts = decodedPicture->pkt_dts;
    emptyPictureBuf->m_width = decodedPicture->width;
    emptyPictureBuf->m_height = decodedPicture->height;
    emptyPictureBuf->m_metadata = decodedPicture->metadata;

    if( useAsyncUpload )
    {
        //posting picture to worker thread
        glTextureUploadThreadPool.start( new AsyncUploader(
            emptyPictureBuf,
            decodedPicture->picData,
            this ) );
        decodedPicture->picData.clear();
    }
    else
    {
        //uploading decodedPicture to gl texture(s)
        updateTextures( emptyPictureBuf, decodedPicture );

        QMutexLocker lk( &m_mutex );
        m_picturesWaitingRendering.push_back( emptyPictureBuf );
    }
}

DecodedPictureToOpenGLUploader::UploadedPicture* DecodedPictureToOpenGLUploader::getUploadedPicture() const
{
    QMutexLocker lk( &m_mutex );

    UploadedPicture* pic = NULL;
    if( !m_picturesWaitingRendering.empty() )
    {
        pic = m_picturesWaitingRendering.front();
        m_picturesWaitingRendering.pop_front();
        NX_LOG( QString::fromAscii( "Taking uploaded picture (pts %1) for first-time rendering" ).arg(pic->pts()), cl_logDEBUG2 );
    }
    else if( !m_renderedPictures.empty() )
    {
        //displaying previous displayed frame, since no new frame available...
        pic = m_renderedPictures.back();
        m_renderedPictures.pop_back();
        NX_LOG( QString::fromAscii( "Taking previously shown uploaded picture (pts %1) for rendering" ).arg(pic->pts()), cl_logDEBUG1 );
    }
    else
    {
        NX_LOG( QString::fromAscii( "Failed to find picture for rendering. No data from decoder?" ), cl_logWARNING );
        return NULL;
    }

    m_picturesBeingRendered.push_back( pic );
    return pic;
}

void DecodedPictureToOpenGLUploader::pictureDrawingFinished( UploadedPicture* const picture ) const
{
    QMutexLocker lk( &m_mutex );

    NX_LOG( QString::fromAscii( "Finished rendering of picture (pts %1)" ).arg(picture->pts()), cl_logDEBUG2 );

    //m_picturesBeingRendered holds only one picture
    std::deque<UploadedPicture*>::iterator it = std::find( m_picturesBeingRendered.begin(), m_picturesBeingRendered.end(), picture );
    Q_ASSERT( it != m_picturesBeingRendered.end() );
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

int DecodedPictureToOpenGLUploader::glRGBFormat() const
{
    if( !isYuvFormat() )
    {
        switch( m_format )
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

bool DecodedPictureToOpenGLUploader::isYuvFormat() const
{
    return m_format == PIX_FMT_YUV422P || m_format == PIX_FMT_YUV420P || m_format == PIX_FMT_YUV444P;
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
    QMutexLocker lk( &m_uploadMutex );
    m_terminated = true;
}

void DecodedPictureToOpenGLUploader::setYV12ToRgbShaderUsed( bool yv12SharedUsed )
{
    m_yv12SharedUsed = yv12SharedUsed;
}

void DecodedPictureToOpenGLUploader::setNV12ToRgbShaderUsed( bool nv12SharedUsed )
{
    m_nv12SharedUsed = nv12SharedUsed;
}

void DecodedPictureToOpenGLUploader::pictureDataUploadSucceeded( const QSharedPointer<QnAbstractPictureDataRef>& /*picData*/, UploadedPicture* const picture )
{
    QMutexLocker lk( &m_mutex );
    m_picturesWaitingRendering.push_back( picture );
}

void DecodedPictureToOpenGLUploader::pictureDataUploadFailed( const QSharedPointer<QnAbstractPictureDataRef>& /*picData*/, UploadedPicture* const picture )
{
    QMutexLocker lk( &m_mutex );
    //considering picture buffer invalid
    m_emptyBuffers.push_back( picture );
}

void DecodedPictureToOpenGLUploader::updateTextures( UploadedPicture* const emptyPictureBuf, const QSharedPointer<CLVideoDecoderOutput>& curImg )
{
    if( curImg->picData.data() )
    {
        switch( curImg->picData->type() )
        {
            case QnAbstractPictureDataRef::pstOpenGL:
                //TODO/IMPL save reference to picture
                return;	//decoded picture is already in OpenGL texture

            case QnAbstractPictureDataRef::pstD3DSurface:
                break;

            default:
                Q_ASSERT( false );
        }
    }

    SafePool<GLContext*, QSharedPointer<GLContext> >::ScopedReadLock glCtxLock(
        emptyPictureBuf->glContextPool(),
        emptyPictureBuf->glContextPool()->lock( emptyPictureBuf->glContext() ) );
    Q_ASSERT( glCtxLock.isValid() );
    uploadDataToGl( glCtxLock->second.data(), emptyPictureBuf, (PixelFormat)curImg->format, curImg->width, curImg->height, curImg->data, curImg->linesize, false );

    //QMutexLocker lk( &m_uploadMutex );
    //uploadDataToGl( m_glContext, emptyPictureBuf, (PixelFormat)curImg->format, curImg->width, curImg->height, curImg->data, curImg->linesize, false );
}

void DecodedPictureToOpenGLUploader::uploadDataToGl(
    GLContext* const glContext,
    UploadedPicture* const emptyPictureBuf,
    const PixelFormat format,
    const unsigned int width,
    const unsigned int height,
    uint8_t* planes[],
    int lineSizes[],
    bool isVideoMemory )
{
    if( m_terminated || !glContext->isValid() )
        return;

    glContext->makeCurrent();

    unsigned int w[3] = { lineSizes[0], lineSizes[1], lineSizes[2] };
    unsigned int r_w[3] = { width, width / 2, width / 2 }; // real_width / visible
    unsigned int h[3] = { height, height / 2, height / 2 };

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

    if( (format == PIX_FMT_YUV420P || format == PIX_FMT_YUV422P || format == PIX_FMT_YUV444P) && usingShaderYuvToRgb() )
    {
        // using pixel shader for yuv->rgb conversion
        for( int i = 0; i < 3; ++i )
        {
            QnGlRendererTexture* texture = emptyPictureBuf->texture(i);
            texture->ensureInitialized( r_w[i], h[i], w[i], 1, GL_LUMINANCE, 1, -1 /*i == 0 ? 0x10 : 0x80*/ );

            glBindTexture(GL_TEXTURE_2D, texture->id());
            const uchar* pixels = planes[i];

            glPixelStorei(GL_UNPACK_ROW_LENGTH, w[i]);
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            qPower2Ceil(r_w[i],ROUND_COEFF),
                            h[i],
                            GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
            bitrateCalculator.bytesProcessed( qPower2Ceil(r_w[i],ROUND_COEFF)*h[i] );
            glCheckError("glTexSubImage2D");
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glCheckError("glPixelStorei");
        }

        emptyPictureBuf->m_colorFormat = PIX_FMT_YUV420P;
    }
    else if( format == PIX_FMT_NV12 && usingShaderNV12ToRgb() )
    {
        for( int i = 0; i < 2; ++i )
        {
            QnGlRendererTexture* texture = emptyPictureBuf->texture(i);
            //void ensureInitialized(int width, int height, int stride, int pixelSize, GLint internalFormat, int internalFormatPixelSize, int fillValue) {
            if( i == 0 )    //Y-plane
                texture->ensureInitialized( width, height, lineSizes[0], 1, GL_LUMINANCE, 1, -1 );
            else
                texture->ensureInitialized( width / 2, height / 2, lineSizes[1], 2, GL_LUMINANCE_ALPHA, 2, -1 );
            glBindTexture( GL_TEXTURE_2D, texture->id() );
            const uchar* pixels = planes[i];
            glPixelStorei( GL_UNPACK_ROW_LENGTH, i == 0 ? lineSizes[0] : (lineSizes[1]/2) );
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            i == 0 ? qPower2Ceil(width,ROUND_COEFF) : width / 2,
                            i == 0 ? height : height / 2,
                            i == 0 ? GL_LUMINANCE : GL_LUMINANCE_ALPHA,
                            GL_UNSIGNED_BYTE, pixels );
            glCheckError("glTexSubImage2D");
            bitrateCalculator.bytesProcessed( (i == 0 ? qPower2Ceil(width,ROUND_COEFF) : width / 2)*(i == 0 ? height : height / 2) );
            //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            //glCheckError("glPixelStorei");
        }

        emptyPictureBuf->m_colorFormat = PIX_FMT_NV12;
    }
    else
    {
        //software conversion data to rgb
        QnGlRendererTexture* texture = emptyPictureBuf->texture(0);

        int bytesPerPixel = 1;
        if( !isYuvFormat() )
        {
            if( format == PIX_FMT_RGB24 || format == PIX_FMT_BGR24 )
                bytesPerPixel = 3;
            else
                bytesPerPixel = 4;
        }

        texture->ensureInitialized(r_w[0], h[0], w[0], bytesPerPixel, GL_RGBA, 4, 0);
        glBindTexture(GL_TEXTURE_2D, texture->id());

        uchar* pixels = planes[0];
        if (isYuvFormat())
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
                        lineSizes[0], lineSizes[1], m_painterOpacity*255);
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
                        lineSizes[0], lineSizes[1], m_painterOpacity*255);
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
                        lineSizes[0], lineSizes[1], m_painterOpacity*255);
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
        glCheckError("glPixelStorei");

        glTexSubImage2D(GL_TEXTURE_2D, 0,
            0, 0,
            qPower2Ceil(r_w[0],ROUND_COEFF),
            h[0],
            glRGBFormat(), GL_UNSIGNED_BYTE, pixels);
        bitrateCalculator.bytesProcessed( qPower2Ceil(r_w[0],ROUND_COEFF)*h[0]*4 );
        glCheckError("glTexSubImage2D");

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glCheckError("glPixelStorei");

        emptyPictureBuf->m_colorFormat = PIX_FMT_RGBA;

        // TODO: free memory immediately for still images
    }

    glContext->doneCurrent();
}

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
        GLContext* ctx = pic->glContext();
        ctx->makeCurrent();
        delete pic;
    }
    pool->clear();
}
