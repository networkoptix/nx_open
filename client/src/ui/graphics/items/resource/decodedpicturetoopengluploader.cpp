/**********************************************************
* 08 oct 2012
* a.kolesnikov
***********************************************************/

#include "decodedpicturetoopengluploader.h"

#include <QMutexLocker>

#ifdef _WIN32
#include <D3D9.h>
#endif

#include <utils/yuvconvert.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/graphics/opengl/gl_context_data.h>


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
            cl_log.log( QString::fromAscii("In previous %1 ms to video mem moved %2 bytes. Transfer rate %3 byte/second").
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

// -------------------------------------------------------------------------- //
// DecodedPictureToOpenGLUploaderPrivate
// -------------------------------------------------------------------------- //
class DecodedPictureToOpenGLUploaderPrivate: public QnGlFunctions
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
        cl_log.log(QString(QLatin1String("OpenGL max texture size: %1.")).arg(maxTextureSize), cl_logINFO);

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
        //glDeleteTextures(1, &m_id);
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
            int fillSize = qMax(textureSize.height(), textureSize.width()) * ROUND_COEFF * internalFormatPixelSize;
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
                bitrateCalculator.bytesProcessed( qMin(ROUND_COEFF, textureSize.width() - roundedWidth)*textureSize.height()*4 );
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
PixelFormat DecodedPictureToOpenGLUploader::UploadedPicture::colorFormat() const
{
    return m_colorFormat;
    //return PIX_FMT_NV12;
    //return PIX_FMT_YUV420P;
    //return PIX_FMT_RGBA;
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

DecodedPictureToOpenGLUploader::UploadedPicture::UploadedPicture( DecodedPictureToOpenGLUploader* const uploader )
:
    m_colorFormat( PIX_FMT_NONE ),
    m_width( 0 ),
    m_height( 0 ),
    m_sequence( 0 ),
    m_pts( 0 )
{
    //TODO/IMPL allocate textures when needed, because not every format has 3 planes
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

inline void streamLoadAndDeinterleaveUVPlane(
    __m128i* nv12UVPlane,
    size_t nv12UVPlaneSize,
    __m128i* yv12UPlane,
    __m128i* yv12VPlane )
{
    static const size_t TMP_BUF_SIZE = 4*1024 / sizeof(__m128i);
    __m128i tmpBuf[TMP_BUF_SIZE];

    //static const __m128i MASK_LO = { 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff };
    static const __m128i MASK_HI = { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00 };
    static const __m128i AND_MASK = MASK_HI;
    static const __m128i SHIFT_8 = { 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    const __m128i* const nv12UVPlaneEnd = nv12UVPlane + nv12UVPlaneSize / sizeof(__m128i);
    while( nv12UVPlane < nv12UVPlaneEnd )
    {
        //reading to tmp buffer
        const size_t bytesToCopy = std::min<size_t>( (nv12UVPlaneEnd - nv12UVPlane)*sizeof(*nv12UVPlane), sizeof(tmpBuf) );
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

            x5 = _mm_packus_epi16( _mm_and_si128( x3, AND_MASK ), _mm_and_si128( x4, AND_MASK ) );   //U
            x6 = _mm_packus_epi16( _mm_srl_epi16( x3, SHIFT_8 ), _mm_srl_epi16( x4, SHIFT_8 ) );   //V
            _mm_store_si128( yv12UPlane+1, x5 );
            _mm_store_si128( yv12VPlane+1, x6 );

            yv12UPlane += 2;
            yv12VPlane += 2;

            nv12UVPlaneTmp += 4;
        }
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
        const QSharedPointer<QnAbstractPictureDataRef>& picData,
        DecodedPictureToOpenGLUploader* uploader )
    :
        m_pictureBuf( pictureBuf ),
        m_picData( picData ),
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
        
    virtual void run()
    {
#ifdef _WIN32
        //checking, if m_picData ref has not been marked for released
        ScopedAtomicLock picUsageCounterLock( &m_picData->syncCtx()->usageCounter );
        if( !m_picData->isValid() ) //m_picData->syncCtx()->sequence != m_sequence
        {
            cl_log.log( QString::fromAscii("AsyncUploader. Frame (pts %1, %2) data ref has been marked for release").
                arg(m_pictureBuf->pts()).arg((size_t)m_picData->syncCtx()), cl_logDEBUG1 );
            m_uploader->pictureDataUploadFailed( m_picData, m_pictureBuf );
            return;
        }

        //TODO/IMPL use cropRect of surface

        Q_ASSERT( m_picData->type() == QnAbstractPictureDataRef::pstD3DSurface );
        IDirect3DSurface9* const surf = static_cast<D3DPictureData*>(m_picData.data())->getSurface();
        D3DSURFACE_DESC surfDesc;
        memset( &surfDesc, 0, sizeof(surfDesc) );
        HRESULT res = surf->GetDesc( &surfDesc );
        if( res != D3D_OK )
        {
            cl_log.log( QString::fromAscii("AsyncUploader. Failed to get dxva surface info (%1). Ignoring decoded picture...").arg(res), cl_logERROR );
            m_uploader->pictureDataUploadFailed( m_picData, m_pictureBuf );
            return;
        }

        if( surfDesc.Format != (D3DFORMAT)MAKEFOURCC('N','V','1','2') )
        {
            cl_log.log( QString::fromAscii("AsyncUploader. Dxva surface format %1 while only NV12 (%2) is supported. Ignoring decoded picture...").
                arg(surfDesc.Format).arg(MAKEFOURCC('N','V','1','2')), cl_logERROR );
            m_uploader->pictureDataUploadFailed( m_picData, m_pictureBuf );
            return;
        }

        D3DLOCKED_RECT lockedRect; 
        memset( &lockedRect, 0, sizeof(lockedRect) );
        res = surf->LockRect( &lockedRect, NULL, D3DLOCK_NOSYSLOCK );
        if( res != D3D_OK )
        {
            cl_log.log( QString::fromAscii("AsyncUploader. Failed to map dxva surface (%1). Ignoring decoded picture...").arg(res), cl_logERROR );
            surf->UnlockRect();
            m_uploader->pictureDataUploadFailed( m_picData, m_pictureBuf );
            return;
        }

        //converting to YV12
        if( m_yv12BufferCapacity < surfDesc.Height * lockedRect.Pitch * 3 / 2 )
        {
            m_yv12BufferCapacity = surfDesc.Height * lockedRect.Pitch * 3 / 2;
            qFreeAligned( m_yv12Buffer );
            m_yv12Buffer = (uint8_t*)qMallocAligned( m_yv12BufferCapacity, sizeof(__m128i) );
        }
        m_planes[0] = m_yv12Buffer;               //Y-plane
        m_lineSizes[0] = lockedRect.Pitch;
        m_planes[1] = m_yv12Buffer + surfDesc.Height * lockedRect.Pitch;          //U-plane
        m_lineSizes[1] = lockedRect.Pitch / 2;  
        m_planes[2] = m_yv12Buffer + surfDesc.Height * lockedRect.Pitch / 4 * 5;  //V-plane
        m_lineSizes[2] = lockedRect.Pitch / 2;
        //TODO/IMPL do not load data that lies to the right from surfDesc.width
        //Y-plane
        memcpy_stream_load(
            (__m128i*)m_planes[0],
            (__m128i*)lockedRect.pBits,
            lockedRect.Pitch * surfDesc.Height );
        streamLoadAndDeinterleaveUVPlane(
            (__m128i*)((uint8_t*)lockedRect.pBits + lockedRect.Pitch * surfDesc.Height),
            lockedRect.Pitch * surfDesc.Height / 2,
            (__m128i*)m_planes[1],
            (__m128i*)m_planes[2] );
        m_uploader->uploadDataToGl( m_pictureBuf, PIX_FMT_YUV420P, surfDesc.Width, surfDesc.Height, m_planes, m_lineSizes, true );

        //assuming surface format is always NV12
        //m_planes[0] = (uint8_t*)lockedRect.pBits;
        //m_lineSizes[0] = lockedRect.Pitch;
        //m_planes[1] = (uint8_t*)lockedRect.pBits + surfDesc.Height * lockedRect.Pitch;
        //m_lineSizes[1] = lockedRect.Pitch;
        //m_uploader->uploadDataToGl( m_pictureBuf, PIX_FMT_NV12, surfDesc.Width, surfDesc.Height, m_planes, m_lineSizes, true );

        surf->UnlockRect();
#endif
        m_uploader->pictureDataUploadSucceeded( m_picData, m_pictureBuf );
    }

private:
    DecodedPictureToOpenGLUploader::UploadedPicture* m_pictureBuf;
    QSharedPointer<QnAbstractPictureDataRef> m_picData;
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
        glTextureUploadThreadPool.setMaxThreadCount( 1 );
    }
};

static GLTextureUploadThreadPoolInitializer gTextureUploadThreadPoolInitializer;

//TODO/IMPL associate gl context with uploading thread, not with DecodedPictureToOpenGLUploader instance so that to be able 
    //to upload simultaneously number of pictures belonging to a single media stream and to remove lock in DecodedPictureToOpenGLUploader::uploadDataToGl
    //??? if we do so we'll have to bind picture buffer to thread

//TODO/IMPL minimize block in \a DecodedPictureToOpenGLUploader::uploadDataToGl method: 
    //before calling this method we have to check that it would not block and take another task if it would

//////////////////////////////////////////////////////////
// DecodedPictureToOpenGLUploader
//////////////////////////////////////////////////////////
//TODO/IMPL we need second frame in case of using async upload to ensure renderer always gets something to draw
static const size_t MIN_GL_PIC_BUF_COUNT = 2;

DecodedPictureToOpenGLUploader::DecodedPictureToOpenGLUploader( const QSharedPointer<QGLContext>& glContext, unsigned int asyncDepth )
:
    m_glContext( glContext ),
    m_format( PIX_FMT_NONE ),
    m_yuv2rgbBuffer( NULL ),
    m_yuv2rgbBufferLen( 0 ),
    m_forceSoftYUV( false ),
    m_painterOpacity( 1.0 ),
    m_previousPicSequence( 1 )
{
    Q_ASSERT( glContext );

    DecodedPictureToOpenGLUploaderPrivateStorage* storage = qn_decodedPictureToOpenGLUploaderPrivateStorage();
    if( storage )
        d = qn_decodedPictureToOpenGLUploaderPrivateStorage()->get( glContext.data() );
    if( d.isNull() )
        d = QSharedPointer<DecodedPictureToOpenGLUploaderPrivate>( new DecodedPictureToOpenGLUploaderPrivate(NULL) );

    for( size_t i = 0; i < asyncDepth+MIN_GL_PIC_BUF_COUNT; ++i )
        m_emptyBuffers.push_back( new UploadedPicture( this ) );
}

DecodedPictureToOpenGLUploader::~DecodedPictureToOpenGLUploader()
{
    QMutexLocker lk( &m_mutex );

    foreach( UploadedPicture* pic, m_emptyBuffers )
        delete pic;
    m_emptyBuffers.clear();
    foreach( UploadedPicture* pic, m_renderedPictures )
        delete pic;
    m_renderedPictures.clear();
    foreach( UploadedPicture* pic, m_picturesWaitingRendering )
        delete pic;
    m_picturesWaitingRendering.clear();
    foreach( UploadedPicture* pic, m_picturesBeingRendered )
        delete pic;
    m_picturesBeingRendered.clear();
}

void DecodedPictureToOpenGLUploader::uploadDecodedPicture( const QSharedPointer<CLVideoDecoderOutput>& decodedPicture )
{
    cl_log.log( QString::fromAscii( "Uploading decoded picture to gl textures. pts %1" ).arg(decodedPicture->pkt_dts), cl_logDEBUG2 );

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
                cl_log.log( QString::fromAscii( "Found empty buffer" ), cl_logDEBUG2 );
            }
            else if( (!useAsyncUpload && !m_renderedPictures.empty())
                  || (useAsyncUpload && (m_renderedPictures.size() > (m_picturesWaitingRendering.empty() ? 1 : 0))) )    //reserving one uploaded picture (preferring picture 
                                                                                                                       //which has not been shown yet) so that renderer always 
                                                                                                                       //gets something to draw...
            {
                //selecting oldest rendered picture
                emptyPictureBuf = m_renderedPictures.front();
                m_renderedPictures.pop_front();
                cl_log.log( QString::fromAscii( "Taking rendered picture (pts %1) buffer for upload. (%2, %3)" ).
                    arg(emptyPictureBuf->pts()).arg(m_renderedPictures.size()).arg(m_picturesWaitingRendering.size()), cl_logDEBUG2 );
            }
            else if( (!useAsyncUpload && !m_picturesWaitingRendering.empty())
                  || (useAsyncUpload && (m_picturesWaitingRendering.size() > (m_renderedPictures.empty() ? 1 : 0))) )
            {
                //looks like rendering does not catch up with decoding. Ignoring oldest decoded frame...
                emptyPictureBuf = m_picturesWaitingRendering.front();
                m_picturesWaitingRendering.pop_front();
                cl_log.log( QString::fromAscii( "Ignoring decoded frame with pts %1. Uploading does not catch up with decoding. (%2, %3)..." ).
                    arg(emptyPictureBuf->pts()).arg(m_renderedPictures.size()).arg(m_picturesWaitingRendering.size()), cl_logINFO );
            }
            else
            {
                if( useAsyncUpload )
                {
                    //ignore decoded picture so that not to stop decoder
                    cl_log.log( QString::fromAscii( "Ignoring decoded frame with pts %1. Uploading does not catch up with decoding..." ).arg(decodedPicture->pkt_dts), cl_logINFO );
                    decodedPicture->picData.clear();
                    return;
                }
                cl_log.log( QString::fromAscii( "Waiting for a picture gl buffer to get free" ), cl_logDEBUG1 );
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
        cl_log.log( QString::fromAscii( "Taking uploaded picture (pts %1) for first-time rendering" ).arg(pic->pts()), cl_logDEBUG2 );
    }
    else if( !m_renderedPictures.empty() )
    {
        //displaying previous displayed frame, since no new frame available...
        pic = m_renderedPictures.back();
        m_renderedPictures.pop_back();
        cl_log.log( QString::fromAscii( "Taking previously shown uploaded picture (pts %1) for rendering" ).arg(pic->pts()), cl_logDEBUG1 );
    }
    else
    {
        cl_log.log( QString::fromAscii( "Failed to find picture for rendering. No data from decoder?" ), cl_logWARNING );
        return NULL;
    }

    m_picturesBeingRendered.push_back( pic );
    return pic;
}

void DecodedPictureToOpenGLUploader::pictureDrawingFinished( UploadedPicture* const picture ) const
{
    QMutexLocker lk( &m_mutex );

    cl_log.log( QString::fromAscii( "Finished rendering of picture (pts %1)" ).arg(picture->pts()), cl_logDEBUG2 );

    //m_picturesBeingRendered holds only one picture
    std::deque<UploadedPicture*>::iterator it = std::find( m_picturesBeingRendered.begin(), m_picturesBeingRendered.end(), picture );
    Q_ASSERT( it != m_picturesBeingRendered.end() );
    m_renderedPictures.push_back( *it );
    m_picturesBeingRendered.erase( it );
    m_cond.wakeAll();
}

const QSharedPointer<QGLContext>& DecodedPictureToOpenGLUploader::getGLContext() const
{
    return m_glContext;
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
    m_glContext->reset();
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

    uploadDataToGl( emptyPictureBuf, (PixelFormat)curImg->format, curImg->width, curImg->height, curImg->data, curImg->linesize, false );
}

void DecodedPictureToOpenGLUploader::uploadDataToGl(
    UploadedPicture* const emptyPictureBuf,
    const PixelFormat format,
    const unsigned int width,
    const unsigned int height,
    uint8_t* planes[],
    int lineSizes[],
    bool isVideoMemory )
{
    QMutexLocker lk( &m_uploadMutex );

    if( !m_glContext->isValid() )
        return;

    m_glContext->makeCurrent();

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

    //TODO/IMPL NV12 support
    if( usingShaderYuvToRgb() )
    {
        // using pixel shader to yuv-> rgb conversion
        for( int i = 0; i < 3; ++i )
        {
            QnGlRendererTexture* texture = emptyPictureBuf->texture(i);
            texture->ensureInitialized( r_w[i], h[i], w[i], 1, GL_LUMINANCE, 1, i == 0 ? 0x10 : 0x80 );

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
                    cl_log.log("CPU does not contain SSE2 module. Color space convert is not implemented", cl_logWARNING);
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
                    cl_log.log("CPU does not contain SSE2 module. Color space convert is not implemented", cl_logWARNING);
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
                    cl_log.log("CPU does not contain SSE2 module. Color space convert is not implemented", cl_logWARNING);
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

    m_glContext->doneCurrent();
}

bool DecodedPictureToOpenGLUploader::usingShaderYuvToRgb() const
{
    return (d->features() & QnGlFunctions::ArbPrograms)
        && (d->features() & QnGlFunctions::OpenGL1_3)
        && !(d->features() & QnGlFunctions::ShadersBroken)
        && !m_forceSoftYUV
        //&& d->m_yv12ToRgbShaderProgram->isValid()
        && isYuvFormat();

    //TODO/IMPL
}
