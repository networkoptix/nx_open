/**********************************************************
* 08 oct 2012
* a.kolesnikov
***********************************************************/

#include "decodedpicturetoopengluploader.h"

#include <algorithm>
#include <memory>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include <QtCore/QRunnable>
#include <QtWidgets/QApplication>

#include <utils/common/util.h> /* For random. */
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <utils/math/math.h>
#include <utils/color_space/yuvconvert.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>
#include "opengl_renderer.h"

#include <nx/streaming/config.h>

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
            NX_DEBUG(this,
                lm("In previous %1 ms to video mem moved %2 Mb. Transfer rate %3 Mb/second.")
                    .arg(currentTick - m_startCalcTick)
                    .arg(m_bytes / 1000000.0)
                    .arg(m_bytes / 1000.0 / (currentTick - m_startCalcTick)));
            m_startCalcTick = currentTick;
            m_bytes = 0;
        }
        m_bytes += byteCount;

        m_mtx.unlock();
    }

private:
    unsigned int m_startCalcTick;
    quint64 m_bytes;
    QnMutex m_mtx;
};

static BitrateCalculator bitrateCalculator;


// -------------------------------------------------------------------------- //
// DecodedPictureToOpenGLUploaderPrivate
// -------------------------------------------------------------------------- //
class DecodedPictureToOpenGLUploaderPrivate: public QOpenGLFunctions
{
    Q_DECLARE_TR_FUNCTIONS(DecodedPictureToOpenGLUploaderPrivate)

public:
    GLint clampConstant;
    bool supportsNonPower2Textures;
    bool forceSoftYUV;
    bool yv12SharedUsed;
    bool nv12SharedUsed;
    QScopedPointer<QnGlFunctions> functions;

    DecodedPictureToOpenGLUploaderPrivate(QOpenGLWidget* glWidget):
        QOpenGLFunctions(glWidget->context()),
        supportsNonPower2Textures(false),
        forceSoftYUV(false),
        yv12SharedUsed(false),
        nv12SharedUsed(false),
        functions(new QnGlFunctions(glWidget))
    {
        initializeOpenGLFunctions();

        /* Clamp constant. */
        clampConstant = GL_CLAMP_TO_EDGE;

        /* Check for non-power of 2 textures. */
        supportsNonPower2Textures = hasOpenGLFeature(OpenGLFeature::NPOTTextures);
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
        QnMutexLocker lk( &m_fillerMutex );

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

    bool usingShaderYuvToRgb() const
    {
        return
            !(functions->features() & QnGlFunctions::ShadersBroken)
            && yv12SharedUsed
            && !forceSoftYUV;
    }

    bool usingShaderNV12ToRgb() const
    {
        return
            !(functions->features() & QnGlFunctions::ShadersBroken)
            && nv12SharedUsed
            && !forceSoftYUV;
    }

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

    QnMutex m_fillerMutex;
    Filler m_fillers[FILLER_COUNT];
};

// -------------------------------------------------------------------------- //
// QnGlRendererTexture
// -------------------------------------------------------------------------- //
class QnGlRendererTexture {

#ifdef _DEBUG
    friend class DecodedPictureToOpenGLUploader;
#endif

public:
    QnGlRendererTexture( const QSharedPointer<DecodedPictureToOpenGLUploaderPrivate>& renderer )
    :
        m_allocated(false),
        m_pixelSize(-1),
        m_internalFormat(-1),
        m_internalFormatPixelSize(-1),
        m_fillValue(-1),
        m_textureSize(QSize(0, 0)),
        m_contentSize(QSize(0, 0)),
        m_id(std::numeric_limits<GLuint>::max()),
        m_renderer(renderer)
    {}

    ~QnGlRendererTexture() {
        //NOTE we do not delete texture here because it belongs to auxiliary gl context which will be removed when these textures are not needed anymore
        if( m_id != std::numeric_limits<GLuint>::max() )
        {
            m_renderer->glDeleteTextures(1, &m_id);
            m_id = std::numeric_limits<GLuint>::max();
        }
    }
    const QSize &textureSize() const {
        return m_textureSize;
    }
    const QVector2D &texCoords() const {
        return m_texCoords;
    }

    GLuint id() const {
        return m_id;
    }

    /*!
        \return true, If uinitialized texture, false is already initialized
    */
    bool ensureInitialized(int width, int height, int stride, int pixelSize, GLint internalFormat, int internalFormatPixelSize, int fillValue) {
        NX_ASSERT(m_renderer.data() != NULL);

        ensureAllocated();

        QSize contentSize = QSize(stride, height);

        if( m_contentSize == contentSize
            && m_pixelSize == pixelSize
            && m_internalFormat == internalFormat
            && m_fillValue == fillValue )
        {
            return false;
        }

        m_pixelSize = pixelSize;
        m_internalFormatPixelSize = internalFormatPixelSize;

        m_contentSize = contentSize;

        QSize textureSize = QSize(
            m_renderer->supportsNonPower2Textures ? qPower2Ceil((unsigned)stride / pixelSize, ROUND_COEFF) : minPow2(stride / pixelSize),
            m_renderer->supportsNonPower2Textures ? height                                   : minPow2(height)
        );

        bool result = false;
        if(m_textureSize.width() != textureSize.width() || m_textureSize.height() != textureSize.height() || m_internalFormat != internalFormat) {

			m_textureSize = textureSize;
            m_internalFormat = internalFormat;

            m_renderer->glBindTexture(GL_TEXTURE_2D, m_id);

            m_renderer->glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, textureSize.width(), textureSize.height(), 0, internalFormat, GL_UNSIGNED_BYTE, NULL);
            m_renderer->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            m_renderer->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            m_renderer->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_renderer->clampConstant);
            m_renderer->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_renderer->clampConstant);

            result = true;
        } else {
            textureSize = m_textureSize;
        }

        //int roundedWidth = qPower2Ceil((unsigned) width, ROUND_COEFF);
        int roundedWidth = textureSize.width();

        m_texCoords = QVector2D(width  / (float) textureSize.width(), height / (float) textureSize.height());

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

            m_renderer->glBindTexture(GL_TEXTURE_2D, m_id);
            if (roundedWidth < textureSize.width()) {

                //NX_ASSERT( textureSize == QSize(textureWidth, textureHeight) );

                m_renderer->glTexSubImage2D(
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
            }

            if (height < textureSize.height()) {
                m_renderer->glTexSubImage2D(
                    GL_TEXTURE_2D,
                    0,
                    0,
                    height,
                    textureSize.width(),
                    qMin(ROUND_COEFF, textureSize.height() - height),
                    internalFormat,
                    GL_UNSIGNED_BYTE,
                    filler );
                bitrateCalculator.bytesProcessed( textureSize.width()*qMin(ROUND_COEFF, textureSize.height() - height)*internalFormatPixelSize );
            }
        }

        return result;
    }

    void ensureAllocated() {
        if(m_allocated)
            return;

        m_renderer->glGenTextures(1, &m_id);
        glCheckError("glGenTextures");

        m_allocated = true;
    }

    void deinitialize()
    {
        //not destroying texture, because it is unknown whether correct gl-context is current
        m_pixelSize = -1;
        m_internalFormat = -1;
        m_internalFormatPixelSize = -1;
        m_fillValue = -1;
        m_textureSize = QSize(0, 0);
        m_contentSize = QSize(0, 0);
    }

private:
    bool m_allocated;
    int m_pixelSize;
    int m_internalFormat;
    int m_internalFormatPixelSize;
    int m_fillValue;
    QSize m_textureSize;
    QSize m_contentSize;
    QVector2D m_texCoords;
    GLubyte m_fillColor;
    GLuint m_id;
    QSharedPointer<DecodedPictureToOpenGLUploaderPrivate> m_renderer;
};


//////////////////////////////////////////////////////////
// QnGlRendererTexturePack
//////////////////////////////////////////////////////////
static const size_t MAX_PLANE_COUNT = 4;
static const size_t Y_PLANE_INDEX = 0;
static const size_t A_PLANE_INDEX = 3;


//!Set of gl textures containing planes of single picture
class QnGlRendererTexturePack
{
public:
    QnGlRendererTexturePack( const QSharedPointer<DecodedPictureToOpenGLUploaderPrivate>& renderer )
    :
        m_format( AV_PIX_FMT_NONE )
    {
        //TODO/IMPL allocate textures when needed, because not every format require 3 planes
        for( size_t i = 0; i < MAX_PLANE_COUNT; ++i )
            m_textures[i].reset( new QnGlRendererTexture(renderer) );
    }

    ~QnGlRendererTexturePack()
    {
    }

    void setPictureFormat(AVPixelFormat format )
    {
        if( m_format == format )
            return;
        m_format = format;
        for( size_t i = 0; i < MAX_PLANE_COUNT; ++i )
            m_textures[i]->deinitialize();
    }

    const std::vector<GLuint>& glTextures() const
    {
        //TODO/IMPL
        m_picTextures.resize( MAX_PLANE_COUNT );
        for( size_t i = 0; i < MAX_PLANE_COUNT; ++i )
            m_picTextures[i] = m_textures[i]->id();
        return m_picTextures;
    }

    QnGlRendererTexture* texture( int index ) const
    {
        return m_textures[index].data();
    }

    QRectF textureRect() const
    {
        const QVector2D& v = m_textures[0]->texCoords();
        return QRectF( 0, 0, v.x(), v.y() );
    }

private:
    mutable std::vector<GLuint> m_picTextures;
    QScopedPointer<QnGlRendererTexture> m_textures[MAX_PLANE_COUNT];
    AVPixelFormat m_format;
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


AVPixelFormat DecodedPictureToOpenGLUploader::UploadedPicture::colorFormat() const
{
    return m_colorFormat;
}

void DecodedPictureToOpenGLUploader::UploadedPicture::setColorFormat(AVPixelFormat newFormat )
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

QRectF DecodedPictureToOpenGLUploader::UploadedPicture::textureRect() const
{
    return m_texturePack->textureRect();
}

const std::vector<GLuint>& DecodedPictureToOpenGLUploader::UploadedPicture::glTextures() const
{
    return m_texturePack->glTextures();
}

unsigned int DecodedPictureToOpenGLUploader::UploadedPicture::sequence() const
{
    return m_sequence;
}

quint64 DecodedPictureToOpenGLUploader::UploadedPicture::pts() const
{
    return m_pts;
}

FrameMetadata DecodedPictureToOpenGLUploader::UploadedPicture::metadata() const
{
    return m_metadata;
}

QnGlRendererTexturePack* DecodedPictureToOpenGLUploader::UploadedPicture::texturePack()
{
    return m_texturePack;
}

QnGlRendererTexture* DecodedPictureToOpenGLUploader::UploadedPicture::texture( int index ) const
{
    return m_texturePack->texture( index );
}

const ImageCorrectionResult& DecodedPictureToOpenGLUploader::UploadedPicture::imageCorrectionResult() const
{
    return m_imgCorrection;
}

void DecodedPictureToOpenGLUploader::UploadedPicture::processImage(
    quint8* yPlane,
    int width,
    int height,
    int stride,
    const nx::vms::api::ImageCorrectionData& data)
{
    m_imgCorrection.analyseImage(yPlane, width, height, stride, data, m_displayedRect);
}

GLuint DecodedPictureToOpenGLUploader::UploadedPicture::pboID( int index ) const
{
    return m_pbo[index].id;
}

int DecodedPictureToOpenGLUploader::UploadedPicture::flags() const
{
    return m_flags;
}

DecodedPictureToOpenGLUploader::UploadedPicture::UploadedPicture( DecodedPictureToOpenGLUploader* const uploader )
:
    m_colorFormat( AV_PIX_FMT_NONE ),
    m_width( 0 ),
    m_height( 0 ),
    m_sequence( 0 ),
    m_pts( 0 ),
    m_skippingForbidden( false ),
    m_flags( 0 ),
    m_displayedRect( 0.0, 0.0, 1.0, 1.0 ),
    m_texturePack( new QnGlRendererTexturePack(uploader->d) )
{
}

DecodedPictureToOpenGLUploader::UploadedPicture::~UploadedPicture()
{
    delete m_texturePack;
    m_texturePack = NULL;
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
// AVPacketUploader
//////////////////////////////////////////////////////////
class AVPacketUploader: public QRunnable
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
        //m_src->setDisplaying( true );
    }

    ~AVPacketUploader()
    {
        //m_src->setDisplaying( false );
    }

    virtual void run()
    {
        m_isRunning = true;

        m_success =
            m_uploader->uploadDataToGl(
                m_dest,
                (AVPixelFormat)m_src->format,
                m_src->width,
                m_src->height,
                m_src->data,
                m_src->linesize,
                false );
        m_done = true;
        if( m_success )
            m_uploader->pictureDataUploadSucceeded(m_dest);
        else
            m_uploader->pictureDataUploadFailed(m_dest );

        nx::vms::api::ImageCorrectionData imCor = m_uploader->getImageCorrection();
        m_dest->processImage(m_src->data[0], m_src->width, m_src->height, m_src->linesize[0], imCor);

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
class DecodedPicturesDeleter: public QRunnable
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

//we need second frame in case of using async upload to ensure renderer always gets something to draw
static const size_t MIN_GL_PIC_BUF_COUNT = 2;

DecodedPictureToOpenGLUploader::DecodedPictureToOpenGLUploader(
    QOpenGLWidget* glWidget,
    unsigned int /*asyncDepth*/ )
:
    d(new DecodedPictureToOpenGLUploaderPrivate(glWidget)),
    m_format( AV_PIX_FMT_NONE ),
    m_yuv2rgbBuffer( NULL ),
    m_yuv2rgbBufferLen( 0 ),
    m_painterOpacity( 1.0 ),
    m_previousPicSequence( 1 ),
    m_terminated( false ),
    m_rgbaBuf( NULL ),
    m_fileNumber( 0 ),
    m_hardwareDecoderUsed( false )
{
    //const int textureQueueSize = asyncDepth+MIN_GL_PIC_BUF_COUNT;
    const int textureQueueSize = 1;

    for( size_t i = 0; i < textureQueueSize; ++i )
        m_emptyBuffers.push_back( new UploadedPicture( this ) );
}

DecodedPictureToOpenGLUploader::~DecodedPictureToOpenGLUploader()
{
    //ensure there is no pointer to the object in the async uploader queue
    discardAllFramesPostedToDisplay();

    pleaseStop();

    delete[] m_rgbaBuf;
    if ( m_yuv2rgbBuffer )
        qFreeAligned(m_yuv2rgbBuffer);
}

void DecodedPictureToOpenGLUploader::pleaseStop()
{
    QnMutexLocker lk( &m_mutex );

    if( m_terminated )
        return;

    m_terminated = true;

    cancelUploadingInGUIThread();   //NOTE this method cannot block if called from GUI thread, so we will never hang GUI thread if called in it

    //TODO/IMPL have to remove gl textures now, while QGLContext surely alive
    while( !m_picturesBeingRendered.empty() )
        m_cond.wait( lk.mutex() );

    const auto previousContext = QOpenGLContext::currentContext();
    QSurface* previousSurface = previousContext ? previousContext->surface() : nullptr;

    const bool differentContext = m_initializedContext && previousContext != m_initializedContext;
    if(differentContext)
        m_initializedContext->makeCurrent(m_initializedSurface);

    releasePictureBuffersNonSafe();

    if(differentContext)
    {
        m_initializedContext->doneCurrent();
        if(previousContext)
            previousContext->makeCurrent(previousSurface);
    }

    m_cond.wakeAll();
}

void DecodedPictureToOpenGLUploader::uploadDecodedPicture(
    const QSharedPointer<CLVideoDecoderOutput>& decodedPicture,
    const QRectF displayedRect )
{
    NX_VERBOSE(this,
        lm("Uploading decoded picture to gl textures. dts %1").arg(decodedPicture->pkt_dts));

    m_hardwareDecoderUsed = decodedPicture->flags & QnAbstractMediaData::MediaFlags_HWDecodingUsed;

    m_format = decodedPicture->format;
    UploadedPicture* emptyPictureBuf = NULL;

    QnMutexLocker lk( &m_mutex );

    if( m_terminated )
        return;

    {
        //searching for a non-used picture buffer
        for( ;; )
        {
            if( m_terminated )
                return;

            if( !m_hardwareDecoderUsed && !m_renderedPictures.empty() )
            {
                //this condition allows to use single PictureBuffer for rendering
                emptyPictureBuf = m_renderedPictures.front();
                m_renderedPictures.pop_front();
                NX_VERBOSE(this,
                    lm("Taking (1) rendered picture (pts %1) buffer for upload (pts %2). (%3, %4)")
                        .arg(emptyPictureBuf->pts())
                        .arg(decodedPicture->pkt_dts)
                        .arg(m_renderedPictures.size())
                        .arg(m_picturesWaitingRendering.size()));
            }
            else if( !m_emptyBuffers.empty() )
            {
                emptyPictureBuf = m_emptyBuffers.front();
                m_emptyBuffers.pop_front();
                NX_VERBOSE(this, "Found empty buffer");
            }
            else if((!m_renderedPictures.empty()))  //reserving one uploaded picture (preferring picture
                                                    //which has not been shown yet) so that renderer always
                                                    //gets something to draw...
            {
                //selecting oldest rendered picture
                emptyPictureBuf = m_renderedPictures.front();
                m_renderedPictures.pop_front();
                NX_VERBOSE(this,
                    lm("Taking (2) rendered picture (pts %1) buffer for upload (pts %2). (%3, %4)")
                        .arg(emptyPictureBuf->pts())
                        .arg(decodedPicture->pkt_dts)
                        .arg(m_renderedPictures.size())
                        .arg(m_picturesWaitingRendering.size()));
            }
            else if( ((!m_picturesWaitingRendering.empty()))
                      && !m_picturesWaitingRendering.front()->m_skippingForbidden )
            {
                //looks like rendering does not catch up with decoding. Ignoring oldest decoded frame...
                emptyPictureBuf = m_picturesWaitingRendering.front();
                m_picturesWaitingRendering.pop_front();
                NX_DEBUG(this,
                    lm("Ignoring uploaded frame with pts %1. Playback does not catch up with uploading. (%2, %3)...")
                        .arg(emptyPictureBuf->pts())
                        .arg(m_renderedPictures.size())
                        .arg(m_picturesWaitingRendering.size()));
            }
            else if(!m_framesWaitingUploadInGUIThread.empty() )
            {
                for( std::deque<AVPacketUploader*>::iterator
                    it = m_framesWaitingUploadInGUIThread.begin();
                    it != m_framesWaitingUploadInGUIThread.end();
                    ++it )
                {
                    if( (*it)->isRunning() || (*it)->picture()->m_skippingForbidden )
                        continue;
                    NX_VERBOSE(this,
                        lm("Ignoring decoded frame with timestamp %1 (%2). Playback does not catch up with decoding.")
                            .arg((*it)->picture()->m_pts)
                            .arg(QDateTime::fromMSecsSinceEpoch((*it)->picture()->m_pts / 1000)
                                .toString(QLatin1String("hh:mm:ss.zzz"))));
                    emptyPictureBuf = (*it)->picture();
                    delete (*it);
                    m_framesWaitingUploadInGUIThread.erase( it );
                    break;
                }
            }

            if( emptyPictureBuf == NULL )
            {
                NX_DEBUG(this, "Waiting for a picture gl buffer to get free");
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
    emptyPictureBuf->m_displayedRect = displayedRect;

    if( decodedPicture->picData )
    {
        NX_ASSERT(false, "Not supported");
    }
    else    //data is stored in system memory (in AVPacket)
    {
        //have go through upload thread, since opengl uploading does not scale good on Intel HD Graphics and
            //it does not matter on PCIe graphics card due to high video memory bandwidth

        m_framesWaitingUploadInGUIThread.push_back( new AVPacketUploader( emptyPictureBuf, decodedPicture, this ) );

        //savePicToFile( decodedPicture.data(), decodedPicture->pkt_dts );

        //if( avPacketUploader->success() )
        //    m_picturesWaitingRendering.push_back( emptyPictureBuf );
        //else
        //    m_emptyBuffers.push_back( emptyPictureBuf );        //considering picture buffer invalid
    }
}

bool DecodedPictureToOpenGLUploader::isUsingFrame( const QSharedPointer<CLVideoDecoderOutput>& image ) const
{
    QnMutexLocker lk( &m_mutex );

    foreach( AVPacketUploader* uploader, m_framesWaitingUploadInGUIThread )
    {
        if( uploader->decodedFrame() == image )
            return true;
    }

    return false;
}

DecodedPictureToOpenGLUploader::UploadedPicture* DecodedPictureToOpenGLUploader::getUploadedPicture() const
{
    QnMutexLocker lk( &m_mutex );

    if( m_terminated )
        return NULL;

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

    UploadedPicture* pic = NULL;
    if( !m_picturesWaitingRendering.empty() )
    {
        pic = m_picturesWaitingRendering.front();
        m_picturesWaitingRendering.pop_front();
        NX_VERBOSE(this,
            lm("Taking uploaded picture (pts %1, seq %2) for first-time rendering.")
                .arg(pic->pts()).arg(pic->m_sequence));
    }
    else if( !m_renderedPictures.empty() )
    {
        //displaying previous displayed frame, since no new frame available...
        pic = m_renderedPictures.back();

        m_renderedPictures.pop_back();
        // NX_VERBOSE(this,
        //    lm("Taking previously shown uploaded picture (pts %1, seq %2) for rendering.")
        //        .arg(pic->pts()).arg(pic->m_sequence));
    }
    else
    {
        NX_VERBOSE(this, "Failed to find picture for rendering. No data from decoder?");
        return NULL;
    }

    m_picturesBeingRendered.push_back( pic );
    lk.unlock();

    //waiting for all gl operations, submitted by uploader are completed by ogl device
    return pic;
}

quint64 DecodedPictureToOpenGLUploader::nextFrameToDisplayTimestamp() const
{
    QnMutexLocker lk( &m_mutex );

    if( !m_picturesBeingRendered.empty() )
        return m_picturesBeingRendered.front()->pts();
    if( !m_picturesWaitingRendering.empty() )
        return m_picturesWaitingRendering.front()->pts();
    if( !m_framesWaitingUploadInGUIThread.empty() )
        return m_framesWaitingUploadInGUIThread.front()->picture()->pts();
    //async upload case (only when HW decoding enabled)
    if( !m_renderedPictures.empty() )
        return m_renderedPictures.back()->pts();
    return AV_NOPTS_VALUE;
}

void DecodedPictureToOpenGLUploader::waitForAllFramesDisplayed()
{
    QnMutexLocker lk( &m_mutex );

    while( !m_terminated && (!m_framesWaitingUploadInGUIThread.empty() || !m_picturesWaitingRendering.empty() || !m_picturesBeingRendered.empty()) )
        m_cond.wait( lk.mutex() );
}

void DecodedPictureToOpenGLUploader::ensureAllFramesWillBeDisplayed()
{
    ensureQueueLessThen(1);
}

void DecodedPictureToOpenGLUploader::ensureQueueLessThen(int maxSize)
{
    if( m_hardwareDecoderUsed )
        return; //if hardware decoder is used, waiting can result in bad playback experience

    QnMutexLocker lk( &m_mutex );

    //MUST wait till all references to frames, supplied via uploadDecodedPicture are not needed anymore
    while( !m_terminated && m_framesWaitingUploadInGUIThread.size() >= static_cast<size_t>(maxSize))
        m_cond.wait( lk.mutex() );

    //for( std::deque<AVPacketUploader*>::iterator
    //    it = m_framesWaitingUploadInGUIThread.begin();
    //    it != m_framesWaitingUploadInGUIThread.end();
    //    ++it )
    //{
    //    (*it)->picture()->m_skippingForbidden = true;
    //}

    //marking, that skipping frames currently in queue is forbidden and exiting...
    for( std::deque<UploadedPicture*>::iterator
        it = m_picturesWaitingRendering.begin();
        it != m_picturesWaitingRendering.end();
        ++it )
    {
        (*it)->m_skippingForbidden = true;
    }
}

void DecodedPictureToOpenGLUploader::discardAllFramesPostedToDisplay()
{
    QnMutexLocker lk( &m_mutex );

    cancelUploadingInGUIThread();

    for( std::deque<UploadedPicture*>::iterator
        it = m_picturesWaitingRendering.begin();
        it != m_picturesWaitingRendering.end() && !m_picturesWaitingRendering.empty();
         )
    {
        m_emptyBuffers.push_back( *it );
        it = m_picturesWaitingRendering.erase( it );
    }

    m_cond.wakeAll();
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
                //NOTE this CANNOT occur, if in GUI thread now...
                ++it;
                continue;
            }
            m_emptyBuffers.push_back( (*it)->picture() );
            delete *it;
            it = m_framesWaitingUploadInGUIThread.erase( it );
        }

        if( m_framesWaitingUploadInGUIThread.empty() )
            break;
        m_cond.wait( &m_mutex );    //NOTE this CANNOT occur, if in GUI thread now...
    }
}

void DecodedPictureToOpenGLUploader::waitForCurrentFrameDisplayed()
{
    QnMutexLocker lk( &m_mutex );

    while( !m_terminated && (!m_picturesBeingRendered.empty()) )
        m_cond.wait( lk.mutex() );
}

void DecodedPictureToOpenGLUploader::pictureDrawingFinished( UploadedPicture* const picture ) const
{
    QnMutexLocker lk( &m_mutex );

    // NX_VERBOSE(this, lm("Finished rendering of picture (pts %1).").arg(picture->pts()));

    //m_picturesBeingRendered holds only one picture
    std::deque<UploadedPicture*>::iterator it = std::find( m_picturesBeingRendered.begin(), m_picturesBeingRendered.end(), picture );
    NX_ASSERT( it != m_picturesBeingRendered.end() );

    m_renderedPictures.push_back( *it );
    m_picturesBeingRendered.erase( it );
    m_cond.wakeAll();
}

void DecodedPictureToOpenGLUploader::setForceSoftYUV( bool value )
{
    d->forceSoftYUV = value;
}

bool DecodedPictureToOpenGLUploader::isForcedSoftYUV() const
{
    return d->forceSoftYUV;
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
    d->yv12SharedUsed = yv12SharedUsed;
}

void DecodedPictureToOpenGLUploader::setNV12ToRgbShaderUsed( bool nv12SharedUsed )
{
    d->nv12SharedUsed = nv12SharedUsed;
}

void DecodedPictureToOpenGLUploader::pictureDataUploadSucceeded(UploadedPicture* const picture )
{
    QnMutexLocker lk(&m_mutex);
    m_picturesWaitingRendering.push_back(picture);
    m_cond.wakeAll();   //notifying that uploading finished
}

void DecodedPictureToOpenGLUploader::pictureDataUploadFailed(UploadedPicture* const picture)
{
    QnMutexLocker lk(&m_mutex);
    if (picture)
        m_emptyBuffers.push_back(picture);

    m_cond.wakeAll();   //notifying that uploading finished
}

static bool isYuvFormat(AVPixelFormat format )
{
    return format == AV_PIX_FMT_YUV422P || format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUV444P;
}

static int glRGBFormat(AVPixelFormat format )
{
    if( !isYuvFormat( format ) )
    {
        switch( format )
        {
            case AV_PIX_FMT_RGBA:
                return GL_RGBA;
            case AV_PIX_FMT_BGRA:
                return GL_BGRA_EXT;
            case AV_PIX_FMT_RGB24:
                return GL_RGB;
            case AV_PIX_FMT_BGR24:
                return GL_BGRA_EXT; // TODO: #asinaisky
            default:
                break;
        }
    }
    return GL_RGBA;
}
static int glBytesPerPixel(AVPixelFormat format )
{
    if( !isYuvFormat( format ) )
    {
        switch( format )
        {
        case AV_PIX_FMT_RGBA:
            return 4;
        case AV_PIX_FMT_BGRA:
            return 4;
        case AV_PIX_FMT_RGB24:
            return 3;
        default:
            break;
        }
    }
    return 4;
}

//void DecodedPictureToOpenGLUploader::setImageCorrection( bool enabled, float blackLevel, float whiteLevel, float gamma)
void DecodedPictureToOpenGLUploader::setImageCorrection(const nx::vms::api::ImageCorrectionData& value)
{
    m_imageCorrection = value;
}

nx::vms::api::ImageCorrectionData DecodedPictureToOpenGLUploader::getImageCorrection() const {
    return m_imageCorrection;
}

/*
static QString toString( AVPixelFormat format )
{
    switch( format )
    {
        case AV_PIX_FMT_YUV444P:
            return lit("AV_PIX_FMT_YUV444P");
        case AV_PIX_FMT_YUV422P:
            return lit("AV_PIX_FMT_YUV422P");
        case AV_PIX_FMT_YUV420P:
            return lit("AV_PIX_FMT_YUV420P");
        default:
            return lit("unknown");
    }
}
*/

bool DecodedPictureToOpenGLUploader::uploadDataToGl(
    DecodedPictureToOpenGLUploader::UploadedPicture* const emptyPictureBuf,
    const AVPixelFormat format,
    const unsigned int width,
    const unsigned int height,
    uint8_t* planes[],
    int lineSizes[],
    bool /*isVideoMemory*/ )
{
    if (!m_initializedContext) // TODO: #vkutin #ynikitenkov Why here?
    {
        m_initializedContext = QOpenGLContext::currentContext();
        m_initializedSurface = m_initializedContext->surface();
    }

    //NX_INFO(this, lm("uploadDataToGl. %1").arg((size_t) this));

    //waiting for all operations with textures (submitted by renderer) are finished

    const int planeCount = format == AV_PIX_FMT_YUVA420P ? 4 : 3;

    unsigned int r_w[MAX_PLANE_COUNT];
    r_w[Y_PLANE_INDEX] = width & (std::numeric_limits<unsigned int>::max() - 1);    //using only odd width to simplify some things
    r_w[1] = width / 2;
    r_w[2] = width / 2;
    r_w[A_PLANE_INDEX] = width; //alpha plane
    unsigned int h[MAX_PLANE_COUNT];
    h[Y_PLANE_INDEX] = height;
    h[1] = height / 2;
    h[2] = height / 2;
    h[A_PLANE_INDEX] = height; //alpha plane

    switch( format )
    {
        case AV_PIX_FMT_YUV444P:
            r_w[1] = r_w[2] = r_w[0];
        // fall through
        case AV_PIX_FMT_YUV422P:
            h[1] = h[2] = height;
            break;
        default:
            break;
    }

    emptyPictureBuf->texturePack()->setPictureFormat( format );

    if( (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUV422P || format == AV_PIX_FMT_YUV444P || format == AV_PIX_FMT_YUVA420P) && usingShaderYuvToRgb() )
    {
        //using pixel shader for yuv->rgb conversion
        for( int i = 0; i < planeCount; ++i )
        {
            QnGlRendererTexture* texture = emptyPictureBuf->texture(i);
            texture->ensureInitialized(
                r_w[i], h[i], lineSizes[i],
                1, GL_LUMINANCE, 1, i == Y_PLANE_INDEX ? 0x10 : (i == A_PLANE_INDEX ? 0x00 : 0x80) );

            NX_VERBOSE(this,
                lm("Uploading to gl texture. id = %1, i = %2, lineSizes[i] = %3, r_w[i] = %4, "
                    "qPower2Ceil(r_w[i],ROUND_COEFF) = %5, h[i] = %6, planes[i] = %7")
                    .arg(texture->id())
                    .arg(i)
                    .arg(lineSizes[i])
                    .arg(r_w[i])
                    .arg(qPower2Ceil(r_w[i],ROUND_COEFF))
                    .arg(h[i])
                    .arg(reinterpret_cast<size_t>(planes[i])));
            d->glBindTexture( GL_TEXTURE_2D, texture->id() );
            glCheckError("glBindTexture");

            const quint64 lineSizes_i = lineSizes[i];
            const quint64 r_w_i = r_w[i];
//            d->glPixelStorei(GL_UNPACK_ROW_LENGTH, lineSizes_i);
            glCheckError("glPixelStorei");
            NX_ASSERT( lineSizes_i >= qPower2Ceil(r_w_i,ROUND_COEFF) );

            loadImageData(d.get(), texture->textureSize().width(),texture->textureSize().height(),lineSizes_i,h[i],1,GL_LUMINANCE,planes[i]);

            bitrateCalculator.bytesProcessed( qPower2Ceil(r_w[i],ROUND_COEFF)*h[i] );
        }

        d->glBindTexture( GL_TEXTURE_2D, 0 );

        emptyPictureBuf->setColorFormat( format == AV_PIX_FMT_YUVA420P ? AV_PIX_FMT_YUVA420P : AV_PIX_FMT_YUV420P );
    }
   else if( format == AV_PIX_FMT_NV12 && usingShaderNV12ToRgb() )
    {
        for( int i = 0; i < 2; ++i )
        {
            QnGlRendererTexture* texture = emptyPictureBuf->texture(i);
            if( i == Y_PLANE_INDEX )
                texture->ensureInitialized( width, height, lineSizes[i], 1, GL_LUMINANCE, 1, -1 );
            else
                texture->ensureInitialized( width / 2, height / 2, lineSizes[i]/2, 2, GL_LUMINANCE_ALPHA, 2, -1 );

            d->glBindTexture( GL_TEXTURE_2D, texture->id() );
            const uchar* pixels = planes[i];
//            d->glPixelStorei( GL_UNPACK_ROW_LENGTH, i == 0 ? lineSizes[0] : (lineSizes[1]/2) );

            loadImageData(d.get(),
                            texture->textureSize().width(),
                            texture->textureSize().height(),
                            i == 0 ? lineSizes[0] : (lineSizes[1]/2),
                            i == 0 ? height : height / 2,
                            i == 0 ? 1 : 2,
                            i == 0 ? GL_LUMINANCE : GL_LUMINANCE_ALPHA,
                            pixels);
            /*
            d->glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            i == 0 ? qPower2Ceil(width,ROUND_COEFF) : width / 2,
                            i == 0 ? height : height / 2,
                            i == 0 ? GL_LUMINANCE : GL_LUMINANCE_ALPHA,
                            GL_UNSIGNED_BYTE, pixels );*/
            glCheckError("glTexSubImage2D");
            d->glBindTexture( GL_TEXTURE_2D, 0 );
            bitrateCalculator.bytesProcessed( (i == 0 ? qPower2Ceil(width,ROUND_COEFF) : width / 2)*(i == 0 ? height : height / 2) );
            //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            //glCheckError("glPixelStorei");
        }

        emptyPictureBuf->setColorFormat( AV_PIX_FMT_NV12 );
    }
    else
    {
        QnMutexLocker lk( &m_uploadMutex );

        //software conversion data to rgb

        int bytesPerPixel = 1;
        if( !isYuvFormat(format) )
        {
            if( format == AV_PIX_FMT_RGB24 || format == AV_PIX_FMT_BGR24 )
                bytesPerPixel = 3;
            else
                bytesPerPixel = 4;
        }

        QnGlRendererTexture* texture = emptyPictureBuf->texture(0);
        texture->ensureInitialized( r_w[0], h[0], lineSizes[0], bytesPerPixel, GL_RGBA, 4, 0 );

        d->glBindTexture(GL_TEXTURE_2D, texture->id());

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
            case AV_PIX_FMT_YUV420P:
                if (useSSE2())
                {
                    yuv420_argb32_simd_intr(pixels, planes[0], planes[2], planes[1],
                        qPower2Ceil(r_w[0],ROUND_COEFF),
                        h[0],
                        4 * lineSizes[0],
                        lineSizes[0], lineSizes[1], opacity()*255);
                }
                else {
                    NX_WARNING(this,
                        "CPU does not contain SSE2 module. Color space convert is not implemented.");
                }
                break;

            case AV_PIX_FMT_YUV422P:
                if (useSSE2())
                {
                    yuv422_argb32_simd_intr(pixels, planes[0], planes[2], planes[1],
                        qPower2Ceil(r_w[0],ROUND_COEFF),
                        h[0],
                        4 * lineSizes[0],
                        lineSizes[0], lineSizes[1], opacity()*255);
                }
                else {
                    NX_WARNING(this,
                        "CPU does not contain SSE2 module. Color space convert is not implemented.");
                }
                break;

            case AV_PIX_FMT_YUV444P:
                if (useSSE2())
                {
                    yuv444_argb32_simd_intr(pixels, planes[0], planes[2], planes[1],
                        qPower2Ceil(r_w[0],ROUND_COEFF),
                        h[0],
                        4 * lineSizes[0],
                        lineSizes[0], lineSizes[1], opacity()*255);
                }
                else {
                    NX_WARNING(this,
                        "CPU does not contain SSE2 module. Color space convert is not implemented.");
                }
                break;

            case AV_PIX_FMT_RGB24:
            case AV_PIX_FMT_BGR24:
                lineInPixelsSize /= 3;
                break;

            default:
                lineInPixelsSize /= 4; // RGBA, BGRA
                break;
        }


//        d->glPixelStorei(GL_UNPACK_ROW_LENGTH, lineInPixelsSize);
        glCheckError("glPixelStorei");

        int w = qPower2Ceil(r_w[0],ROUND_COEFF);
        int gl_bytes_per_pixel = glBytesPerPixel(format);
        int gl_format = glRGBFormat(format);

        loadImageData(d.get(), texture->textureSize().width(),texture->textureSize().height(),lineInPixelsSize,h[0],gl_bytes_per_pixel,gl_format,pixels);
        bitrateCalculator.bytesProcessed( w*h[0]*4 );
        glCheckError("glTexSubImage2D");

//        d->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glCheckError("glPixelStorei");

        d->glBindTexture( GL_TEXTURE_2D, 0 );

        emptyPictureBuf->setColorFormat( AV_PIX_FMT_RGBA );

        // TODO: #ak free memory immediately for still images
    }

    if (m_hardwareDecoderUsed)
    {
        d->glFlush();
        d->glFinish();
    }

    return true;
}

bool DecodedPictureToOpenGLUploader::usingShaderYuvToRgb() const
{
    return d->usingShaderYuvToRgb();
}

bool DecodedPictureToOpenGLUploader::usingShaderNV12ToRgb() const
{
    return d->usingShaderNV12ToRgb();
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
        glCheckError("glGenBuffers");
        picBuf->m_pbo[pboIndex].sizeBytes = 0;
    }

    if( picBuf->m_pbo[pboIndex].sizeBytes < sizeInBytes )
    {
//        d->glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, picBuf->m_pbo[pboIndex].id );
        glCheckError("glBindBuffer");
//GL_STREAM_DRAW_ARB
//GL_STREAM_READ_ARB
//GL_STREAM_COPY_ARB
//GL_STATIC_DRAW_ARB
//GL_STATIC_READ_ARB
//GL_STATIC_COPY_ARB
//GL_DYNAMIC_DRAW_ARB
//GL_DYNAMIC_READ_ARB
//GL_DYNAMIC_COPY_ARB
//        d->glBufferData( GL_PIXEL_UNPACK_BUFFER_ARB, sizeInBytes, NULL, GL_STREAM_DRAW_ARB/*GL_STATIC_DRAW_ARB*/ );
        glCheckError("glBufferData");
//        d->glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );
        glCheckError("glBindBuffer");
        picBuf->m_pbo[pboIndex].sizeBytes = sizeInBytes;
    }
}

void DecodedPictureToOpenGLUploader::releasePictureBuffers()
{
    QnMutexLocker lk( &m_mutex );
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

static void yv12aToRgba(
    int width, int height,
    quint8* yp, size_t y_stride,
    quint8* up, size_t u_stride,
    quint8* vp, size_t v_stride,
    quint8* ap, size_t a_stride,
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
            (rgbaBuf + (y*width + x)*PIXEL_SIZE)[3] = ap ? *(ap+y*a_stride+x) : 0xff;
        }
    }
}

void DecodedPictureToOpenGLUploader::savePicToFile( AVFrame* const pic, int pts )
{
    if( !m_rgbaBuf )
        m_rgbaBuf = new uint8_t[pic->width*pic->height*4];

    yv12aToRgba(
        pic->width, pic->height,
        pic->data[0], pic->linesize[0],
        pic->data[1], pic->linesize[1],
        pic->data[2], pic->linesize[2],
        pic->data[3], pic->linesize[3],
        m_rgbaBuf );

    QImage img(
        m_rgbaBuf,
        pic->width,
        pic->height,
        QImage::Format_ARGB32 ); //QImage::Format_ARGB4444_Premultiplied );
    const QString& fileName = lit("C:\\temp\\%1_%2.png").arg(m_fileNumber++, 3, 10, QLatin1Char('0')).arg(pts);
    img.save(fileName, "png");
    /*if( !img.save( fileName, "bmp" ) )
        int x = 0;*/
}
