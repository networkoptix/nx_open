#include "gl_renderer.h"
#include <cassert>
#include <QCoreApplication> /* For Q_DECLARE_TR_FUNCTIONS. */
#include <QMessageBox>
#include <QErrorMessage>
#include <QScopedPointer>
#include <QMutex>
#include <utils/common/warnings.h>
#include <utils/common/util.h>
#include <utils/media/sse_helper.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/shaders/yuy2_to_rgb_shader_program.h>
#include <ui/graphics/shaders/yv12_to_rgb_shader_program.h>
#include "utils/yuvconvert.h"
#include "camera.h"

#ifdef QN_GL_RENDERER_DEBUG_PERFORMANCE
#   include <utils/common/performance.h>
#endif

#define QN_GL_RENDERER_DEBUG

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

    static bool qn_openGlInfoWrittenOut = false;

    class OpenGlErrorMessageDisplay: public QObject {
    public:
        virtual ~OpenGlErrorMessageDisplay() {
            const QString message = tr("We have detected that your video card drivers may be not installed or out of date.\n"
                "Installing and/or updating your video drivers can substantially increase your system performance when viewing and working with video.\n"
                "For easy instructions on how to install or update your video driver, follow instruction at http://tribaltrouble.com/driversupport.php");

            QMessageBox::critical(NULL, tr("Important Performance Tip"), message, QMessageBox::Ok);
        }
    };

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnGLRendererPrivate
// -------------------------------------------------------------------------- //
class QnGLRendererPrivate: public QnGlFunctions
{
    Q_DECLARE_TR_FUNCTIONS(QnGLRendererPrivate);

public:
    static int getMaxTextureSize() { return maxTextureSize; }

    QnGLRendererPrivate(const QGLContext *context):
        QnGlFunctions(context),
        supportsNonPower2Textures(false),
        status(QnGLRenderer::SUPPORTED)
    {
        QByteArray extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        QByteArray version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        QByteArray renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

        /* Write out OpenGL info. */
        if(!qn_openGlInfoWrittenOut) {
            qn_openGlInfoWrittenOut = true;

            cl_log.log(QString(QLatin1String("OpenGL extensions: %1.")).arg(QLatin1String(extensions.constData())), cl_logINFO);
            cl_log.log(QString(QLatin1String("OpenGL version: %1.")).arg(QLatin1String(version.constData())), cl_logINFO);
            cl_log.log(QString(QLatin1String("OpenGL renderer: %1.")).arg(QLatin1String(renderer.constData())), cl_logINFO);
            cl_log.log(QString(QLatin1String("OpenGL vendor: %1.")).arg(QLatin1String(vendor.constData())), cl_logINFO);

            bool showMessageBox = false;

            if (!(features() & QnGlFunctions::OpenGL1_3)) {
                qnWarning("Multitexturing is not supported.");
                showMessageBox = true;
            }

            if (version <= QByteArray("1.1.0")) {
                qnWarning("OpenGL version %1 is not supported.", version);
                showMessageBox = true;
            }

            if (!(features() & QnGlFunctions::ArbPrograms)) {
                qnWarning("OpenGL ARB shaders not supported, using software YUV to RGB conversion.");
                showMessageBox = true;
            }

            if(showMessageBox) {
                OpenGlErrorMessageDisplay *messageDisplay = new OpenGlErrorMessageDisplay();
                messageDisplay->deleteLater(); /* Message will be shown in destructor, close to the event loop. */
            }
        }

        if(!(features() & QnGlFunctions::ArbPrograms))
            status = QnGLRenderer::NOT_SUPPORTED; /* In this first revision we do not support software color transform. */

        /* Maximal texture size. */
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        cl_log.log(QString(QLatin1String("OpenGL max texture size: %1.")).arg(maxTextureSize), cl_logINFO);

        /* Clamp constant. */
        clampConstant = GL_CLAMP;
        if (extensions.contains("GL_EXT_texture_edge_clamp")) {
            clampConstant = GL_CLAMP_TO_EDGE_EXT;
        } else if (extensions.contains("GL_SGIS_texture_edge_clamp")) {
            clampConstant = GL_CLAMP_TO_EDGE_SGIS;
        } else if (version >= QByteArray("1.2.0")) {
            clampConstant = GL_CLAMP_TO_EDGE;
        }

        /* Check for non-power of 2 textures. */
        supportsNonPower2Textures = extensions.contains("GL_ARB_texture_non_power_of_two");

        /* Prepare shaders. */
        m_yuy2ToRgbShaderProgram.reset(new QnYuy2ToRgbShaderProgram(context));
        m_yv12ToRgbShaderProgram.reset(new QnYv12ToRgbShaderProgram(context));
    }

    uchar *filler(uchar value, int size) {
        QMutexLocker locker(&fillerMutex);

        QVector<uchar> &filler = fillers[value];

        if(filler.size() < size) {
            filler.resize(size);
            filler.fill(value);
        }

        return &filler[0];
    }

public:
    QnGLRenderer::HardwareStatus status;

    GLint clampConstant;
    bool supportsNonPower2Textures;
    static int maxTextureSize;

    QScopedPointer<QnYuy2ToRgbShaderProgram> m_yuy2ToRgbShaderProgram;
    QScopedPointer<QnYv12ToRgbShaderProgram> m_yv12ToRgbShaderProgram;

private:
    QMutex fillerMutex;
    QVector<uchar> fillers[256];
};

int QnGLRendererPrivate::maxTextureSize = 0;

typedef QnGlContextData<QnGLRendererPrivate, QnGlContextDataForwardingFactory<QnGLRendererPrivate> > QnGLRendererPrivateStorage;
Q_GLOBAL_STATIC(QnGLRendererPrivateStorage, qn_glRendererPrivateStorage);


// -------------------------------------------------------------------------- //
// QnGlRendererTexture
// -------------------------------------------------------------------------- //
class QnGlRendererTexture {
public:
    QnGlRendererTexture(): 
        m_allocated(false),
        m_internalFormat(-1),
        m_textureSize(QSize(0, 0)),
        m_contentSize(QSize(0, 0)),
        m_id(-1),
        m_fillValue(-1),
        m_renderer(NULL)
    {}

    ~QnGlRendererTexture() {
        //glDeleteTextures(3, m_textures);

        // I do not know why but if I glDeleteTextures here some items on the other view might become green( especially if we animate them a lot )
        // not sure I i do something wrong with opengl or it's bug of QT. for now can not spend much time on it. but it needs to be fixed.
        if(m_allocated)
            QnGLRenderer::m_garbage.append(m_id); /* To delete later. */
    }

    void setRenderer(QnGLRenderer *renderer) {
        m_renderer = renderer;
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
        assert(m_renderer != NULL);

        ensureAllocated();

        QSize contentSize = QSize(width, height);

        if(m_internalFormat == internalFormat && m_fillValue == fillValue && m_contentSize == contentSize)
            return;

        m_contentSize = contentSize;

        QSharedPointer<QnGLRendererPrivate> d = m_renderer->d;

        QSize textureSize = QSize(
            d->supportsNonPower2Textures ? qPower2Ceil((unsigned)stride / pixelSize, ROUND_COEFF) : minPow2(stride / pixelSize),
            d->supportsNonPower2Textures ? height                                   : minPow2(height)
        );

        if(m_textureSize.width() < textureSize.width() || m_textureSize.height() < textureSize.height() || m_internalFormat != internalFormat) {
            m_textureSize = textureSize;
            m_internalFormat = internalFormat;

            glBindTexture(GL_TEXTURE_2D, m_id);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, textureSize.width(), textureSize.height(), 0, internalFormat, GL_UNSIGNED_BYTE, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, d->clampConstant);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, d->clampConstant);
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
            uchar *filler = d->filler(fillValue, fillSize);

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
                    filler
                );
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
                    filler
                );
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
    QnGLRenderer *m_renderer;
};


// -------------------------------------------------------------------------- //
// QnGLRenderer
// -------------------------------------------------------------------------- //
QList<GLuint> QnGLRenderer::m_garbage;

int QnGLRenderer::maxTextureSize() 
{
    return QnGLRendererPrivate::getMaxTextureSize();
}

void QnGLRenderer::clearGarbage()
{
    foreach (GLuint textureId, m_garbage)
        glDeleteTextures(1, &textureId);
    m_garbage.clear();
}

bool QnGLRenderer::isPixelFormatSupported(PixelFormat pixfmt)
{
    switch (pixfmt)
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
        break;
    };
    return false;
}

QnGLRenderer::QnGLRenderer(const QGLContext *context)
{
    /* Postpone private initialization until it is actually needed.
     * This way if context is NULL, then we will pick the right context when
     * paint event is delivered. */
    m_context = context;
    m_glInitialized = false;

    m_forceSoftYUV = false;
    m_textureUploaded = false;
    m_stride_old = 0;
    m_height_old = 0;
    m_brightness = 0;
    m_contrast = 0;
    m_hue = 0;
    m_saturation = 0;
    m_painterOpacity = 1.0;
    m_needwait = true;
    m_curImg = 0;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_newtexture = false;

    m_yuv2rgbBuffer = 0;
    m_yuv2rgbBufferLen = 0;

    m_lastDisplayedFlags = 0;
    
    for(int i = 0; i < TEXTURE_COUNT; i++) {
        m_textures[i].reset(new QnGlRendererTexture());
        m_textures[i]->setRenderer(this);
    }

    applyMixerSettings(m_brightness, m_contrast, m_hue, m_saturation);
}

QnGLRenderer::~QnGLRenderer()
{
    qFreeAligned(m_yuv2rgbBuffer);
}

void QnGLRenderer::ensureGlInitialized() {
    if(m_glInitialized)
        return;

    if(m_context == NULL)
        m_context = QGLContext::currentContext();

    QnGLRendererPrivateStorage *storage = qn_glRendererPrivateStorage();
    if(storage)
        d = qn_glRendererPrivateStorage()->get(m_context);
    if(d.isNull()) /* Application is being shut down. */
        d = QSharedPointer<QnGLRendererPrivate>(new QnGLRendererPrivate(NULL));

    m_glInitialized = true;
}

void QnGLRenderer::beforeDestroy()
{
    QMutexLocker lock(&m_displaySync);
    m_needwait = false;
    if (m_curImg)
        m_curImg->setDisplaying(false);
    m_waitCon.wakeAll();
}

void QnGLRenderer::draw(CLVideoDecoderOutput *img)
{
    QMutexLocker locker(&m_displaySync);

    //m_imageList.enqueue(img);
    if (m_curImg) 
        m_curImg->setDisplaying(false);
    m_curImg = img;
    m_format = m_curImg->format;


    m_curImg->setDisplaying(true);
    if (m_curImg->linesize[0] != m_stride_old || m_curImg->height != m_height_old || m_curImg->format != m_color_old)
    {
        m_stride_old = m_curImg->linesize[0];
        m_height_old = m_curImg->height;
        m_color_old = (PixelFormat) m_curImg->format;
    }

}

void QnGLRenderer::waitForFrameDisplayed(int channel)
{
    Q_UNUSED(channel)

    QMutexLocker lock(&m_displaySync);
    while (m_needwait && m_curImg && m_curImg->isDisplaying()) 
    {
        m_waitCon.wait(&m_displaySync);
    }
}

void QnGLRenderer::setForceSoftYUV(bool value)
{
    m_forceSoftYUV = value;
}

qreal QnGLRenderer::opacity() const
{
    return m_painterOpacity;
}

void QnGLRenderer::setOpacity(qreal opacity)
{
    m_painterOpacity = opacity;
}

void QnGLRenderer::applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation)
{
    // normalize the values
    m_brightness = brightness * 128;
    m_contrast = contrast + 1.0;
    m_hue = hue * 180.;
    m_saturation = saturation + 1.0;
}

int QnGLRenderer::glRGBFormat() const
{
    if (!isYuvFormat())
    {
        switch (m_format)
        {
        case PIX_FMT_RGBA: return GL_RGBA;
        case PIX_FMT_BGRA: return GL_BGRA_EXT;
        case PIX_FMT_RGB24: return GL_RGB;
        case PIX_FMT_BGR24: return GL_BGR_EXT;
        default: break;
        }
    }
    return GL_RGBA;
}

bool QnGLRenderer::isYuvFormat() const
{
    return m_format == PIX_FMT_YUV422P || m_format == PIX_FMT_YUV420P || m_format == PIX_FMT_YUV444P;
}

void QnGLRenderer::updateTexture()
{
    unsigned int w[3] = { m_curImg->linesize[0], m_curImg->linesize[1], m_curImg->linesize[2] };
    unsigned int r_w[3] = { m_curImg->width, m_curImg->width / 2, m_curImg->width / 2 }; // real_width / visible
    unsigned int h[3] = { m_curImg->height, m_curImg->height / 2, m_curImg->height / 2 };

    switch (m_curImg->format)
    {
    case PIX_FMT_YUV444P:
        r_w[1] = r_w[2] = m_curImg->width;
    // fall through
    case PIX_FMT_YUV422P:
        h[1] = h[2] = m_curImg->height;
        break;
    default:
        break;
    }

    if (usingShaderYuvToRgb())
    {
        // using pixel shader to yuv-> rgb conversion
        for (int i = 0; i < 3; ++i)
        {
            QnGlRendererTexture *texture = this->texture(i);
            texture->ensureInitialized(r_w[i], h[i], w[i], 1, GL_LUMINANCE, 1, i == 0 ? 0x10 : 0x80);

            glBindTexture(GL_TEXTURE_2D, texture->id());
            const uchar *pixels = m_curImg->data[i];

            glPixelStorei(GL_UNPACK_ROW_LENGTH, w[i]);

#ifdef QN_GL_RENDERER_DEBUG_PERFORMANCE
            qint64 frequency = QnPerformance::currentCpuFrequency();
            qint64 startCycles = QnPerformance::currentThreadCycles();
#endif 

            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            qPower2Ceil(r_w[i],ROUND_COEFF),
                            h[i],
                            GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);

#ifdef QN_GL_RENDERER_DEBUG_PERFORMANCE
            qint64 deltaCycles = QnPerformance::currentThreadCycles() - startCycles;
            qDebug() << "glTexSubImage2D" << deltaCycles / (frequency / 1000.0) << "ms" << deltaCycles;
#endif 

            glCheckError("glTexSubImage2D");
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glCheckError("glPixelStorei");
        }

        m_textureUploaded = true;
    }
    else
    {
        QnGlRendererTexture *texture = this->texture(0);

        int bytesPerPixel = 1;
        if (!isYuvFormat()) {
            if (m_curImg->format == PIX_FMT_RGB24 || m_curImg->format == PIX_FMT_BGR24)
                bytesPerPixel = 3;
            else
                bytesPerPixel = 4;
        }

        texture->ensureInitialized(r_w[0], h[0], w[0], bytesPerPixel, GL_RGBA, 4, 0);
        glBindTexture(GL_TEXTURE_2D, texture->id());

        uchar *pixels = m_curImg->data[0];
        if (isYuvFormat())
        {
            int size = 4 * m_curImg->linesize[0] * h[0];
            if (m_yuv2rgbBufferLen < size)
            {
                m_yuv2rgbBufferLen = size;
                qFreeAligned(m_yuv2rgbBuffer);
                m_yuv2rgbBuffer = (uchar*)qMallocAligned(size, CL_MEDIA_ALIGNMENT);
            }
            pixels = m_yuv2rgbBuffer;
        }

        int lineInPixelsSize = m_curImg->linesize[0];
        switch (m_curImg->format)
        {
        case PIX_FMT_YUV420P:
            if (useSSE2())
            {
                yuv420_argb32_sse2_intr(pixels, m_curImg->data[0], m_curImg->data[2], m_curImg->data[1],
                    qPower2Ceil(r_w[0],ROUND_COEFF),
                    h[0],
                    4 * m_curImg->linesize[0],
                    m_curImg->linesize[0], m_curImg->linesize[1], m_painterOpacity*255);
            }
            else {
                cl_log.log("CPU does not contains SSE2 module. Color space convert is not implemented", cl_logWARNING);
            }
            break;

        case PIX_FMT_YUV422P:
            if (useSSE2())
            {
                yuv422_argb32_sse2_intr(pixels, m_curImg->data[0], m_curImg->data[2], m_curImg->data[1],
                    qPower2Ceil(r_w[0],ROUND_COEFF),
                    h[0],
                    4 * m_curImg->linesize[0],
                    m_curImg->linesize[0], m_curImg->linesize[1], m_painterOpacity*255);
            }
            else {
                cl_log.log("CPU does not contains SSE2 module. Color space convert is not implemented", cl_logWARNING);
            }
            break;

        case PIX_FMT_YUV444P:
            if (useSSE2())
            {
                yuv444_argb32_sse2_intr(pixels, m_curImg->data[0], m_curImg->data[2], m_curImg->data[1],
                    qPower2Ceil(r_w[0],ROUND_COEFF),
                    h[0],
                    4 * m_curImg->linesize[0],
                    m_curImg->linesize[0], m_curImg->linesize[1], m_painterOpacity*255);
            }
            else {
                cl_log.log("CPU does not contains SSE2 module. Color space convert is not implemented", cl_logWARNING);
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
        glCheckError("glTexSubImage2D");

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glCheckError("glPixelStorei");

        // TODO: free memory immediately for still images
    }
}

void QnGLRenderer::drawVideoTexture(QnGlRendererTexture *tex0, QnGlRendererTexture *tex1, QnGlRendererTexture *tex2, const float* v_array)
{
    float tx_array[8] = {
        0.0f, 0.0f,
        tex0->texCoords().x(), 0.0f,
        tex0->texCoords().x(), tex0->texCoords().y(),
        0.0f, tex0->texCoords().y()
    };

    glEnable(GL_TEXTURE_2D);
    glCheckError("glEnable");

    QnYv12ToRgbShaderProgram *prog = NULL;
    if (usingShaderYuvToRgb()) {
        prog = d->m_yv12ToRgbShaderProgram.data();

        prog->bind();
        prog->setParameters(m_brightness / 256.0f, m_contrast, m_hue, m_saturation, m_painterOpacity);

        d->glActiveTexture(GL_TEXTURE2);
        glCheckError("glActiveTexture");
        glBindTexture(GL_TEXTURE_2D, tex2->id());
        glCheckError("glBindTexture");

        d->glActiveTexture(GL_TEXTURE1);
        glCheckError("glActiveTexture");
        glBindTexture(GL_TEXTURE_2D, tex1->id());
        glCheckError("glBindTexture");

        d->glActiveTexture(GL_TEXTURE0);
        glCheckError("glActiveTexture");
        glBindTexture(GL_TEXTURE_2D, tex0->id());
        glCheckError("glBindTexture");
    } else {
        glBindTexture(GL_TEXTURE_2D, tex0->id());
        glCheckError("glBindTexture");
    }

    glVertexPointer(2, GL_FLOAT, 0, v_array);
    glCheckError("glVertexPointer");
    glTexCoordPointer(2, GL_FLOAT, 0, tx_array);
    glCheckError("glTexCoordPointer");
    glEnableClientState(GL_VERTEX_ARRAY);
    glCheckError("glEnableClientState");
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glCheckError("glEnableClientState");
    glDrawArrays(GL_QUADS, 0, 4);
    glCheckError("glDrawArrays");
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glCheckError("glDisableClientState");
    glDisableClientState(GL_VERTEX_ARRAY);
    glCheckError("glDisableClientState");

    if (prog != NULL)
        prog->release();
}

CLVideoDecoderOutput *QnGLRenderer::update()
{
    QMutexLocker locker(&m_displaySync);

    if (m_curImg && m_curImg->linesize[0]) {
        m_videoWidth = m_curImg->width;
        m_videoHeight = m_curImg->height;
        updateTexture();
        if (m_curImg->pkt_dts != AV_NOPTS_VALUE)
            m_lastDisplayedTime = m_curImg->pkt_dts;
        m_lastDisplayedMetadata[m_curImg->channel] = m_curImg->metadata;
        m_lastDisplayedFlags = m_curImg->flags;
        m_newtexture = true;
    } 

    return m_curImg;
}

Qn::RenderStatus QnGLRenderer::paint(const QRectF &r)
{
    ensureGlInitialized();

    //glPushAttrib(GL_ALL_ATTRIB_BITS);
    /*
    if (m_painterOpacity < 1.0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    */

    Qn::RenderStatus result;

    CLVideoDecoderOutput *curImg = update();
    if(m_newtexture) {
        result = Qn::NewFrameRendered;
    } else {
        result = Qn::OldFrameRendered;
    }
    m_newtexture = false;

    bool draw = m_videoWidth <= maxTextureSize() && m_videoHeight <= maxTextureSize();
    if(!draw) 
    {
        result = Qn::CannotRender;
    } 
    else if(m_videoWidth > 0 && m_videoHeight > 0)
    {
        QRectF temp(r);
        const float v_array[] = { temp.left(), temp.top(), temp.right(), temp.top(), temp.right(), temp.bottom(), temp.left(), temp.bottom() };
        drawVideoTexture(texture(0), texture(1), texture(2), v_array);
    }
    else
    {
        result = Qn::NothingRendered;
    }

    QMutexLocker locker(&m_displaySync);
    if (curImg && curImg == m_curImg && curImg->isDisplaying()) {
        curImg->setDisplaying(false);
        m_curImg = 0;
        m_waitCon.wakeAll();
    }

    //glPopAttrib();

    return result;
}

QnGlRendererTexture *QnGLRenderer::texture(int index) {
    return m_textures[index].data();
}

qint64 QnGLRenderer::lastDisplayedTime() const
{
    QMutexLocker locker(&m_displaySync);
    return m_lastDisplayedTime;
}

bool QnGLRenderer::isLowQualityImage() const
{
    return m_lastDisplayedFlags & QnAbstractMediaData::MediaFlags_LowQuality;
}

QnMetaDataV1Ptr QnGLRenderer::lastFrameMetadata(int channel) const
{
    QMutexLocker locker(&m_displaySync);
    return m_lastDisplayedMetadata[channel];
}

bool QnGLRenderer::usingShaderYuvToRgb() const {
    return (d->features() & QnGlFunctions::ArbPrograms) && (d->features() & QnGlFunctions::OpenGL1_3) && !m_forceSoftYUV && isYuvFormat() && 
		d->m_yv12ToRgbShaderProgram->isValid() && !(d->features() & QnGlFunctions::ShadersBroken);
}
