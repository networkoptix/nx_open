#include "gl_renderer.h"

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
    const unsigned int ROUND_COEFF = 8;

    unsigned int minPow2(unsigned int value)
    {
        unsigned int result = 1;
        while (value > result)
            result<<=1;

        return result;
    }

    typedef void (APIENTRY *PFNActiveTexture) (GLenum);

} // anonymous namespace


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

        if (!QnArbShaderProgram::hasArbShaderPrograms()) {
            qnWarning("OpenGL ARB shaders not supported, using software YUV to RGB conversion.");
            supportsArbShaders = false;

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
    m_initialized = false;
    m_isSoftYuv2Rgb = false;
    m_forceSoftYUV = false;
    m_textureUploaded = false;
    m_stride_old = 0;
    m_height_old = 0;
    m_videoTextureReady = false;
    m_brightness = 0;
    m_contrast = 0;
    m_hue = 0;
    m_saturation = 0;
    m_painterOpacity = 1.0;
    m_gotnewimage = false;
    m_needwait = true;
    m_initialized = false;
    m_curImg = 0;
    m_videoWidth = 0;
    m_videoHeight = 0;

    m_textureStep = 0;

    applyMixerSettings(m_brightness, m_contrast, m_hue, m_saturation);
}

QnGLRenderer::~QnGLRenderer()
{
    if (m_textureUploaded)
    {
        //glDeleteTextures(3, m_texture);

        // I do not know why but if I glDeleteTextures here some items on the other view might become green( especially if we animate them a lot )
        // not sure I i do something wrong with opengl or it's bug of QT. for now can not spend much time on it. but it needs to be fixed.
        for(int i = 0; i < 6; i++)
            m_garbage.append(m_texture[i]); /* To delete later. */
    }
}

void QnGLRenderer::beforeDestroy()
{
    QMutexLocker lock(&m_displaySync);
    m_needwait = false;
    if (m_curImg)
        m_curImg->setDisplaying(false);
    m_waitCon.wakeAll();
}

void QnGLRenderer::ensureInitialized()
{
    if (m_initialized)
        return;

    glGenTextures(6, m_texture);
    OGL_CHECK_ERROR("glGenTextures");

    m_initialized = true;
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
        m_videoTextureReady = false;
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
    //image.saveToFile("test.yuv");

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

    glEnable(GL_TEXTURE_2D);
    OGL_CHECK_ERROR("glEnable");
    if (!m_isSoftYuv2Rgb && isYuvFormat())
    {
        // using pixel shader to yuv-> rgb conversion
        for (int i = 0; i < 3; ++i)
        {
            glBindTexture(GL_TEXTURE_2D, m_texture[i]);
            OGL_CHECK_ERROR("glBindTexture");
            const uchar* pixels = m_curImg->data[i];
            if (!m_videoTextureReady)
            {
                // if support "GL_ARB_texture_non_power_of_two", use default size of texture,
                // else nearest power of two
                unsigned int wPow = m_isNonPower2 ? roundUp(w[i],ROUND_COEFF) : minPow2(w[i]);
                unsigned int hPow = m_isNonPower2 ? h[i] : minPow2(h[i]);
                // support GL_ARB_texture_non_power_of_two ?

                m_videoCoeffL[i] = 0;
                unsigned int round_r_w = roundUp(r_w[i],ROUND_COEFF);
                m_videoCoeffW[i] =  round_r_w / (float) wPow;

                m_videoCoeffH[i] = h[i] / (float) hPow;

                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, wPow, hPow, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_clampConstant);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_clampConstant);

                // by default uninitialized YUV texture has green color. Fill right and bottom black bars
                // due to GL_LINEAR filtering, openGL uses some "unvisible" pixels, so it is unvisible pixels MUST be black
                int fillSize = qMax(round_r_w, h[i]) * ROUND_COEFF;
                uchar *filler;
                if (i == 0)
                    filler = m_shared->yFiller(fillSize);
                else
                    filler = m_shared->uvFiller(fillSize);

                glPixelStorei(GL_UNPACK_ROW_LENGTH, ROUND_COEFF);
                if (round_r_w < wPow)
                {
                    glTexSubImage2D(GL_TEXTURE_2D, 0,
                        round_r_w,
                        0,
                        qMin(ROUND_COEFF, wPow - round_r_w),
                        h[i],
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, filler);
                    OGL_CHECK_ERROR("glTexSubImage2D");
                }
                if (h[i] < hPow)
                {
                    glTexSubImage2D(GL_TEXTURE_2D, 0,
                        0, h[i],
                        qMin(round_r_w + ROUND_COEFF, wPow),
                        qMin(h[i] + ROUND_COEFF, hPow),
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, filler);
                    OGL_CHECK_ERROR("glTexSubImage2D");
                }
            }

            glPixelStorei(GL_UNPACK_ROW_LENGTH, w[i]);

            qint64 frequency = QnPerformance::currentCpuFrequency();
            qint64 startCycles = QnPerformance::currentThreadCycles();

            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            roundUp(r_w[i],ROUND_COEFF),
                            h[i],
                            GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);

            qint64 deltaCycles = QnPerformance::currentThreadCycles() - startCycles;

            //qDebug() << "UpdateTexture" << deltaCycles / (frequency / 1000.0) << "ms" << deltaCycles;


            OGL_CHECK_ERROR("glTexSubImage2D");
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            OGL_CHECK_ERROR("glPixelStorei");
        }

        m_textureUploaded = true;
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, m_texture[0]);

        //const int wPow = m_isNonPower2 ? roundUp(w[0]) : minPow2(w[0]);
        //const int hPow = m_isNonPower2 ? h[0] : minPow2(h[0]);

        if (!m_videoTextureReady)
        {
            // if support "GL_ARB_texture_non_power_of_two", use default size of texture,
            // else nearest power of two
            int bytesPerPixel = 1;
            if (!isYuvFormat()) {
                if (m_curImg->format == PIX_FMT_RGB24 || m_curImg->format == PIX_FMT_BGR24)
                    bytesPerPixel = 3;
                else
                    bytesPerPixel = 4;
            }
            const int wPow = m_isNonPower2 ? roundUp(w[0]/bytesPerPixel,ROUND_COEFF) : minPow2(w[0]/bytesPerPixel);
            const int hPow = m_isNonPower2 ? h[0] : minPow2(h[0]);
            // support GL_ARB_texture_non_power_of_two ?
            m_videoCoeffL[0] = 0;
            m_videoCoeffW[0] = roundUp(r_w[0],ROUND_COEFF) / float(wPow);
            m_videoCoeffH[0] = h[0] / float(hPow);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, wPow, hPow, 0, glRGBFormat(), GL_UNSIGNED_BYTE, 0);
            OGL_CHECK_ERROR("glTexImage2D");
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            OGL_CHECK_ERROR("glTexParameteri");
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            OGL_CHECK_ERROR("glTexParameteri");
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_clampConstant);
            OGL_CHECK_ERROR("glTexParameteri");
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_clampConstant);
            OGL_CHECK_ERROR("glTexParameteri");
        }

        uchar *pixels = m_curImg->data[0];
        if (isYuvFormat())
        {
            int size = 4 * m_curImg->linesize[0] * h[0];
            if (m_yuv2rgbBuffer.size() < size)
                m_yuv2rgbBuffer.resize(size);
            pixels = m_yuv2rgbBuffer.data();
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
    m_videoTextureReady = true;
    m_gotnewimage = false;

    /*
    if (!m_isSoftYuv2Rgb)
    {
        glActiveTexture(GL_TEXTURE0);glDisable (GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE1);glDisable (GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE2);glDisable (GL_TEXTURE_2D);
    }
    */
}

void QnGLRenderer::drawVideoTexture(GLuint tex0, GLuint tex1, GLuint tex2, const float* v_array)
{
    PFNActiveTexture glActiveTexture = m_shared->glActiveTexture;

    float tx_array[8] = {
        m_videoCoeffL[0], 0.0f,
        m_videoCoeffW[0], 0.0f,
        m_videoCoeffW[0], m_videoCoeffH[0],
        m_videoCoeffL[0], m_videoCoeffH[0]
    };

    glEnable(GL_TEXTURE_2D);
    OGL_CHECK_ERROR("glEnable");

    QnYv12ToRgbShaderProgram *prog = NULL;
    if (!m_isSoftYuv2Rgb && isYuvFormat()) {
        prog = m_shared->m_yv12ToRgbShaderProgram.data();

        prog->bind();
        prog->setParameters(m_brightness / 256.0f, m_contrast, m_hue, m_saturation, m_painterOpacity);

        glActiveTexture(GL_TEXTURE2);
        OGL_CHECK_ERROR("glActiveTexture");
        glBindTexture(GL_TEXTURE_2D, tex2);
        OGL_CHECK_ERROR("glBindTexture");

        glActiveTexture(GL_TEXTURE1);
        OGL_CHECK_ERROR("glActiveTexture");
        glBindTexture(GL_TEXTURE_2D, tex1);
        OGL_CHECK_ERROR("glBindTexture");

        glActiveTexture(GL_TEXTURE0);
        OGL_CHECK_ERROR("glActiveTexture");
        glBindTexture(GL_TEXTURE_2D, tex0);
        OGL_CHECK_ERROR("glBindTexture");
    } else {
        glBindTexture(GL_TEXTURE_2D, tex0);
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

QnGLRenderer::RenderStatus QnGLRenderer::paintEvent(const QRectF &r)
{
    ensureInitialized();

    glPushAttrib(GL_ALL_ATTRIB_BITS);

    if (m_painterOpacity < 1.0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    RenderStatus result;

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

            result = RENDERED_NEW_FRAME;
        } 
        else 
        {
            result = RENDERED_OLD_FRAME;
        }
    }

    bool draw = m_videoWidth <= maxTextureSize() && m_videoHeight <= maxTextureSize();
    if(!draw) 
    {
        result = CANNOT_RENDER;
    } 
    else if(m_videoWidth > 0 && m_videoHeight > 0)
    {
        QRectF temp(r);
        const float v_array[] = { temp.left(), temp.top(), temp.right(), temp.top(), temp.right(), temp.bottom(), temp.left(), temp.bottom() };
        drawVideoTexture(m_texture[0], m_texture[1], m_texture[2], v_array);
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












