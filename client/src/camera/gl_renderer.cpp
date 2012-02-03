#include "gl_renderer.h"
#include <cassert>
#include <QCoreApplication> /* For Q_DECLARE_TR_FUNCTIONS. */
#include <QMessageBox>
#include <QScopedPointer>
#include <QMutex>
#include <utils/common/warnings.h>
#include <utils/common/util.h>
#include <utils/media/sse_helper.h>
#include <utils/common/performance.h>
#include <ui/graphics/shaders/yuy2_to_rgb_shader_program.h>
#include <ui/graphics/shaders/yv12_to_rgb_shader_program.h>
#include "yuvconvert.h"
#include "camera.h"
#include "ui/common/opengl.h"

#ifndef GL_CLAMP_TO_EDGE
#   define GL_CLAMP_TO_EDGE    0x812F
#endif

#ifndef GL_CLAMP_TO_EDGE_SGIS
#   define GL_CLAMP_TO_EDGE_SGIS 0x812F
#endif

#ifndef GL_CLAMP_TO_EDGE_EXT
#   define GL_CLAMP_TO_EDGE_EXT 0x812F
#endif

/* Support old OpenGL installations (1.2).
 * assume that if TEXTURE0 isn't defined, none are */
#ifndef GL_TEXTURE0
#   define GL_TEXTURE0    0x84C0
#   define GL_TEXTURE1    0x84C1
#   define GL_TEXTURE2    0x84C2
#endif

#ifndef APIENTRY
#   define APIENTRY
#endif

#define OGL_CHECK_ERROR(str) //if (glCheckError() != GL_NO_ERROR) {cl_log.log(str, __LINE__ , cl_logERROR); }

namespace {
    const int ROUND_COEFF = 8;

    int minPow2(int value)
    {
        int result = 1;
        while (value > result)
            result <<= 1;

        return result;
    }

    typedef void (APIENTRY *PFNActiveTexture) (GLenum);

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnGLRendererSharedData
// -------------------------------------------------------------------------- //
class QnGLRendererSharedData {
    Q_DECLARE_TR_FUNCTIONS(QnGLRendererSharedData);

public:
    QnGLRendererSharedData():
        supportsArbShaders(false),
        supportsNonPower2Textures(false),
        status(QnGLRenderer::SUPPORTED),
        maxTextureSize(0)
    {
        glActiveTexture = (PFNActiveTexture) QGLContext::currentContext()->getProcAddress(QLatin1String("glActiveTexture"));
        if (!glActiveTexture)
            CL_LOG(cl_logWARNING) cl_log.log(QLatin1String("GL_ARB_multitexture not supported"), cl_logWARNING);

        /* OpenGL info. */
        QByteArray extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        qnDebug("OpenGL extensions: %1.", extensions);

        QByteArray version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        qnDebug("OpenGL version: %1.", version);

        QByteArray renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        qnDebug("OpenGL renderer: %1.", renderer);

        QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
        qnDebug("OpenGL vendor: %1.", vendor);

        if (version <= QByteArray("1.1.0")) {
            const QString message = tr("OpenGL driver is not installed or outdated. Please update video driver for better perfomance.");
            qnWarning("%1", message);
            QMessageBox::warning(0, tr("Important Performance Tip"), message, QMessageBox::Ok, QMessageBox::NoButton);
        }

        /* Maximal texture size. */
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        qnDebug("OpenGL max texture size: %1.", maxTextureSize);

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
        m_yuy2ToRgbShaderProgram.reset(new QnYuy2ToRgbShaderProgram());
        m_yv12ToRgbShaderProgram.reset(new QnYv12ToRgbShaderProgram());

        if (QnArbShaderProgram::hasArbShaderPrograms()) {
            supportsArbShaders = true;
        } else {
            supportsArbShaders = false;
            qnWarning("OpenGL ARB shaders not supported, using software YUV to RGB conversion.");

            /* In this first revision we do not support software color transform. */
            status = QnGLRenderer::NOT_SUPPORTED;

            const QString message = tr("We have detected that your video card drivers may be not installed or out of date.\n"
                                       "Installing and/or updating your video drivers can substantially increase your system performance when viewing and working with video.\n"
                                       "For easy instructions on how to install or update your video driver, follow instruction at http://tribaltrouble.com/driversupport.php");
            QMessageBox::warning(0, tr("Important Performance Tip"), message, QMessageBox::Ok, QMessageBox::NoButton);
        }
    }

    uchar *yFiller(int size) {
        QMutexLocker locker(&fillerMutex);

        if(yFillerData.size() < size) {
            yFillerData.resize(size);
            yFillerData.fill(0x10);
        }

        return &yFillerData[0];
    }

    uchar *uvFiller(int size) {
        QMutexLocker locker(&fillerMutex);

        if(uvFillerData.size() < size) {
            uvFillerData.resize(size);
            uvFillerData.fill(0x80);
        }

        return &uvFillerData[0];
    }

public:
    QnGLRenderer::HardwareStatus status;

    PFNActiveTexture glActiveTexture;
    GLint clampConstant;
    bool supportsNonPower2Textures;
    bool supportsArbShaders;
    int maxTextureSize;

    QScopedPointer<QnYuy2ToRgbShaderProgram> m_yuy2ToRgbShaderProgram;
    QScopedPointer<QnYv12ToRgbShaderProgram> m_yv12ToRgbShaderProgram;

private:
    QMutex fillerMutex;
    QVector<uchar> yFillerData;
    QVector<uchar> uvFillerData;
};

Q_GLOBAL_STATIC(QnGLRendererSharedData, qn_glRendererSharedData);


// -------------------------------------------------------------------------- //
// QnGlRendererTexture
// -------------------------------------------------------------------------- //
class QnGlRendererTexture {
public:
    enum FillMode {
        NO_FILL,
        Y_FILL,
        UV_FILL
    };

    QnGlRendererTexture(): 
        m_allocated(false),
        m_internalFormat(-1),
        m_textureSize(QSize(0, 0)),
        m_contentSize(QSize(0, 0)),
        m_id(-1),
        m_fillMode(NO_FILL)
    {}

    ~QnGlRendererTexture() {
        //glDeleteTextures(3, m_textures);

        // I do not know why but if I glDeleteTextures here some items on the other view might become green( especially if we animate them a lot )
        // not sure I i do something wrong with opengl or it's bug of QT. for now can not spend much time on it. but it needs to be fixed.
        if(m_allocated)
            QnGLRenderer::m_garbage.append(m_id); /* To delete later. */
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

    void ensureInitialized(int width, int height, int stride, int pixelSize, GLint internalFormat, FillMode fillMode) {
        ensureAllocated();

        QSize contentSize = QSize(width, height);

        if(m_internalFormat == internalFormat && m_fillMode == fillMode && m_contentSize == contentSize)
            return;

        m_contentSize = contentSize;

        QnGLRendererSharedData *d = qn_glRendererSharedData();

        QSize textureSize = QSize(
            d->supportsNonPower2Textures ? roundUp(stride / pixelSize, ROUND_COEFF) : minPow2(stride / pixelSize),
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

            m_fillMode = NO_FILL; /* So that the texture is re-filled. */
        } else {
            textureSize = m_textureSize;
        }

        int roundedWidth = roundUp(width, ROUND_COEFF);
        m_texCoords = QVector2D(
            static_cast<float>(roundedWidth) / textureSize.width(),
            static_cast<float>(height) / textureSize.height()
        );

        if(fillMode != m_fillMode) {
            m_fillMode = fillMode;

            // by default uninitialized YUV texture has green color. Fill right and bottom black bars
            // due to GL_LINEAR filtering, openGL uses some "unvisible" pixels, so it is unvisible pixels MUST be black
            int fillSize = qMax(textureSize.height(), textureSize.width()) * ROUND_COEFF;
            uchar *filler;
            if (fillMode == Y_FILL)
                filler = d->yFiller(fillSize);
            else
                filler = d->uvFiller(fillSize);

            glPixelStorei(GL_UNPACK_ROW_LENGTH, ROUND_COEFF);
            if (roundedWidth < textureSize.width()) {
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
                OGL_CHECK_ERROR("glTexSubImage2D");
            }

            if (height < textureSize.height()) {
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
                OGL_CHECK_ERROR("glTexSubImage2D");
            }
        }
    }

    void ensureAllocated() {
        if(m_allocated)
            return;

        glGenTextures(1, &m_id);
        OGL_CHECK_ERROR("glGenTextures");

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
    FillMode m_fillMode;
};


// -------------------------------------------------------------------------- //
// QnGLRenderer
// -------------------------------------------------------------------------- //
QList<GLuint> QnGLRenderer::m_garbage;

int QnGLRenderer::maxTextureSize() {
    return qn_glRendererSharedData()->maxTextureSize;
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

QnGLRenderer::QnGLRenderer():
    m_shared(qn_glRendererSharedData())
{
    m_forceSoftYUV = false;
    m_textureUploaded = false;
    m_stride_old = 0;
    m_height_old = 0;
    m_brightness = 0;
    m_contrast = 0;
    m_hue = 0;
    m_saturation = 0;
    m_painterOpacity = 1.0;
    m_gotnewimage = false;
    m_needwait = true;
    m_curImg = 0;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_newtexture = false;
    m_updated = false;

    m_yuv2rgbBuffer = 0;
    m_yuv2rgbBufferLen = 0;
    
    for(int i = 0; i < TEXTURE_COUNT; i++)
        m_textures[i].reset(new QnGlRendererTexture());

    applyMixerSettings(m_brightness, m_contrast, m_hue, m_saturation);
}

QnGLRenderer::~QnGLRenderer()
{
    qFreeAligned(m_yuv2rgbBuffer);
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
    //m_abort_drawing = false;

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

    m_gotnewimage = true;
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

    //glEnable(GL_TEXTURE_2D);
    OGL_CHECK_ERROR("glEnable");
    if ((m_shared->supportsArbShaders && !m_forceSoftYUV) && isYuvFormat())
    {
        // using pixel shader to yuv-> rgb conversion
        for (int i = 0; i < 3; ++i)
        {
            QnGlRendererTexture *texture = this->texture(i);
            texture->ensureInitialized(r_w[i], h[i], w[i], 1, GL_LUMINANCE, i == 0 ? QnGlRendererTexture::Y_FILL : QnGlRendererTexture::UV_FILL);

            glBindTexture(GL_TEXTURE_2D, texture->id());
            const uchar *pixels = m_curImg->data[i];

            glPixelStorei(GL_UNPACK_ROW_LENGTH, w[i]);

            qint64 frequency = QnPerformance::currentCpuFrequency();
            qint64 startCycles = QnPerformance::currentThreadCycles();

            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            roundUp(r_w[i],ROUND_COEFF),
                            h[i],
                            GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);

            qint64 deltaCycles = QnPerformance::currentThreadCycles() - startCycles;

            qDebug() << "UpdateTexture" << deltaCycles / (frequency / 1000.0) << "ms" << deltaCycles << texture->id();


            OGL_CHECK_ERROR("glTexSubImage2D");
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            OGL_CHECK_ERROR("glPixelStorei");
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

        texture->ensureInitialized(r_w[0], h[0], w[0], bytesPerPixel, GL_RGBA, QnGlRendererTexture::NO_FILL);
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
                yuv420_argb32_sse_intr(pixels, m_curImg->data[0], m_curImg->data[2], m_curImg->data[1],
                    roundUp(r_w[0],ROUND_COEFF),
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
                yuv422_argb32_sse_intr(pixels, m_curImg->data[0], m_curImg->data[2], m_curImg->data[1],
                    roundUp(r_w[0],ROUND_COEFF),
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
                yuv444_argb32_sse_intr(pixels, m_curImg->data[0], m_curImg->data[2], m_curImg->data[1],
                    roundUp(r_w[0],ROUND_COEFF),
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
        OGL_CHECK_ERROR("glPixelStorei");

        glTexSubImage2D(GL_TEXTURE_2D, 0,
            0, 0,
            roundUp(r_w[0],ROUND_COEFF),
            h[0],
            glRGBFormat(), GL_UNSIGNED_BYTE, pixels);

        OGL_CHECK_ERROR("glTexSubImage2D");
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        OGL_CHECK_ERROR("glPixelStorei");

        // TODO: free memory immediately for still images
    }

    //glDisable(GL_TEXTURE_2D);
    m_gotnewimage = false;
}

void QnGLRenderer::drawVideoTexture(QnGlRendererTexture *tex0, QnGlRendererTexture *tex1, QnGlRendererTexture *tex2, const float* v_array)
{
    PFNActiveTexture glActiveTexture = m_shared->glActiveTexture;

    float tx_array[8] = {
        0.0f, 0.0f,
        tex0->texCoords().x(), 0.0f,
        tex0->texCoords().x(), tex0->texCoords().y(),
        0.0f, tex0->texCoords().y()
    };

    glEnable(GL_TEXTURE_2D);
    OGL_CHECK_ERROR("glEnable");

    QnYv12ToRgbShaderProgram *prog = NULL;
    if (m_shared->supportsArbShaders && !m_forceSoftYUV && isYuvFormat()) {
        prog = m_shared->m_yv12ToRgbShaderProgram.data();

        prog->bind();
        prog->setParameters(m_brightness / 256.0f, m_contrast, m_hue, m_saturation, m_painterOpacity);

        glActiveTexture(GL_TEXTURE2);
        OGL_CHECK_ERROR("glActiveTexture");
        glBindTexture(GL_TEXTURE_2D, tex2->id());
        OGL_CHECK_ERROR("glBindTexture");

        glActiveTexture(GL_TEXTURE1);
        OGL_CHECK_ERROR("glActiveTexture");
        glBindTexture(GL_TEXTURE_2D, tex1->id());
        OGL_CHECK_ERROR("glBindTexture");

        glActiveTexture(GL_TEXTURE0);
        OGL_CHECK_ERROR("glActiveTexture");
        glBindTexture(GL_TEXTURE_2D, tex0->id());
        OGL_CHECK_ERROR("glBindTexture");
    } else {
        glBindTexture(GL_TEXTURE_2D, tex0->id());
        OGL_CHECK_ERROR("glBindTexture");
    }

    glVertexPointer(2, GL_FLOAT, 0, v_array);
    OGL_CHECK_ERROR("glVertexPointer");
    glTexCoordPointer(2, GL_FLOAT, 0, tx_array);
    OGL_CHECK_ERROR("glTexCoordPointer");
    glEnableClientState(GL_VERTEX_ARRAY);
    OGL_CHECK_ERROR("glEnableClientState");
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    OGL_CHECK_ERROR("glEnableClientState");
    glDrawArrays(GL_QUADS, 0, 4);
    OGL_CHECK_ERROR("glDrawArrays");
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    OGL_CHECK_ERROR("glDisableClientState");
    glDisableClientState(GL_VERTEX_ARRAY);
    OGL_CHECK_ERROR("glDisableClientState");

    if (prog != NULL)
        prog->release();
}

void QnGLRenderer::update()
{
    CLVideoDecoderOutput *curImg;
    {
        QMutexLocker locker(&m_displaySync);
        curImg = m_curImg;
        if (m_gotnewimage && curImg && curImg->linesize[0]) {
            m_videoWidth = curImg->width;
            m_videoHeight = curImg->height;
            updateTexture();
            if (curImg->pkt_dts != AV_NOPTS_VALUE)
                m_lastDisplayedTime = curImg->pkt_dts;
            m_lastDisplayedMetadata = curImg->metadata;
            m_newtexture = true;
            m_textureImg = curImg;
        } 
    }

    m_updated = true;
}

QnGLRenderer::RenderStatus QnGLRenderer::paint(const QRectF &r)
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    if (m_painterOpacity < 1.0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    RenderStatus result;

    if(!m_updated)
        update();
    m_updated = false;

    CLVideoDecoderOutput *curImg = m_textureImg;
    if(m_newtexture) {
        result = RENDERED_NEW_FRAME;
    } else {
        result = RENDERED_NEW_FRAME;
    }
    m_newtexture = false;

    bool draw = m_videoWidth <= maxTextureSize() && m_videoHeight <= maxTextureSize();
    if(!draw) 
    {
        result = CANNOT_RENDER;
    } 
    else if(m_videoWidth > 0 && m_videoHeight > 0)
    {
        QRectF temp(r);
        const float v_array[] = { temp.left(), temp.top(), temp.right(), temp.top(), temp.right(), temp.bottom(), temp.left(), temp.bottom() };
        drawVideoTexture(texture(0), texture(1), texture(2), v_array);
    }
    else
    {
        result = NOTHING_RENDERED;
    }

    QMutexLocker locker(&m_displaySync);
    if (curImg && curImg == m_curImg && curImg->isDisplaying()) {
        curImg->setDisplaying(false);
        m_curImg = 0;
        m_waitCon.wakeAll();
    }

    glPopAttrib();

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

QnMetaDataV1Ptr QnGLRenderer::lastFrameMetadata() const
{
    QMutexLocker locker(&m_displaySync);
    return m_lastDisplayedMetadata;
}












