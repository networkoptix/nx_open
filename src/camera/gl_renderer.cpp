#include "gl_renderer.h"

#include <QtCore/qmath.h>
//#include <QtCore/QTime>

#include <QtGui/QMessageBox>

#include "../base/log.h"
#include "ui/videoitem/video_wnd_item.h"
#include "yuvconvert.h"
#include "camera.h"
#include "device/device.h"
#include "util.h"

#ifndef GL_FRAGMENT_PROGRAM_ARB
#define GL_FRAGMENT_PROGRAM_ARB           0x8804
#define GL_PROGRAM_FORMAT_ASCII_ARB       0x8875
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE    0x812F
#endif

#ifndef GL_CLAMP_TO_EDGE_SGIS
#define GL_CLAMP_TO_EDGE_SGIS 0x812F
#endif

#ifndef GL_CLAMP_TO_EDGE_EXT
#define GL_CLAMP_TO_EDGE_EXT 0x812F
#endif

// support old OpenGL installations (1.2)
// assume that if TEXTURE0 isn't defined, none are

#ifndef GL_TEXTURE0
# define GL_TEXTURE0    0x84C0
# define GL_TEXTURE1    0x84C1
# define GL_TEXTURE2    0x84C2
#endif

static const int MAX_SHADER_SIZE = 1024*3;
static const int ROUND_COEFF = 8;

QVector<uchar> CLGLRenderer::m_staticYFiller;
QVector<uchar> CLGLRenderer::m_staticUVFiller;

QMutex CLGLRenderer::m_programMutex;
bool CLGLRenderer::m_programInited = false;
GLuint CLGLRenderer::m_program[2];


#define OGL_CHECK_ERROR(str) //if (checkOpenGLError() != GL_NO_ERROR) {cl_log.log(str, __LINE__ , cl_logERROR); }

// arbfp1 fragment program for converting yuv (YV12) to rgb
static const char yv12ToRgb[] =
"!!ARBfp1.0"
"PARAM c[5] = { program.local[0..1],"
"                { 1.164, 0, 1.596, 0.5 },"
"                { 0.0625, 1.164, -0.391, -0.81300002 },"
"                { 1.164, 2.0179999, 0 } };"
"TEMP R0;"
"TEX R0.x, fragment.texcoord[0], texture[1], 2D;"
"ADD R0.y, R0.x, -c[2].w;"
"TEX R0.x, fragment.texcoord[0], texture[2], 2D;"
"ADD R0.x, R0, -c[2].w;"
"MUL R0.z, R0.y, c[0].w;"
"MAD R0.z, R0.x, c[0], R0;"
"MUL R0.w, R0.x, c[0];"
"MUL R0.z, R0, c[0].y;"
"TEX R0.x, fragment.texcoord[0], texture[0], 2D;"
"MAD R0.y, R0, c[0].z, R0.w;"
"ADD R0.x, R0, -c[3];"
"MUL R0.y, R0, c[0];"
"MUL R0.z, R0, c[1].x;"
"MAD R0.x, R0, c[0].y, c[0];"
"MUL R0.y, R0, c[1].x;"
"DP3 result.color.x, R0, c[2];"
"DP3 result.color.y, R0, c[3].yzww;"
"DP3 result.color.z, R0, c[4];"
"MOV result.color.w, c[1].y;"
"END";

static const char yuy2ToRgb[] =
"!!ARBfp1.0"
"PARAM c[5] = { program.local[0..1],"
"                { 0.5, 2, 1, 0.0625 },"
"                { 1.164, 0, 1.596, 2.0179999 },"
"                { 1.164, -0.391, -0.81300002 } };"
"TEMP R0;"
"TEMP R1;"
"TEMP R2;"
"FLR R1.z, fragment.texcoord[0].x;"
"ADD R0.x, R1.z, c[2];"
"ADD R1.z, fragment.texcoord[0].x, -R1;"
"MUL R1.x, fragment.texcoord[0].z, R0;"
"MOV R1.y, fragment.texcoord[0];"
"TEX R0, R1, texture[0], 2D;"
"ADD R1.y, R0.z, -R0.x;"
"MUL R2.x, R1.z, R1.y;"
"MAD R0.x, R2, c[2].y, R0;"
"MOV R1.y, fragment.texcoord[0];"
"ADD R1.x, fragment.texcoord[0].z, R1;"
"TEX R1.xyw, R1, texture[0], 2D;"
"ADD R2.x, R1, -R0.z;"
"MAD R1.x, R1.z, c[2].y, -c[2].z;"
"MAD R0.z, R1.x, R2.x, R0;"
"ADD R1.xy, R1.ywzw, -R0.ywzw;"
"ADD R0.z, R0, -R0.x;"
"SGE R1.w, R1.z, c[2].x;"
"MAD R0.x, R1.w, R0.z, R0;"
"MAD R0.yz, R1.z, R1.xxyw, R0.xyww;"
"ADD R0.xyz, R0, -c[2].wxxw;"
"MUL R0.w, R0.y, c[0];"
"MAD R0.w, R0.z, c[0].z, R0;"
"MUL R0.z, R0, c[0].w;"
"MAD R0.y, R0, c[0].z, R0.z;"
"MUL R0.w, R0, c[0].y;"
"MUL R0.y, R0, c[0];"
"MUL R0.z, R0.w, c[1].x;"
"MAD R0.x, R0, c[0].y, c[0];"
"MUL R0.y, R0, c[1].x;"
"DP3 result.color.x, R0, c[3];"
"DP3 result.color.y, R0, c[4];"
"DP3 result.color.z, R0, c[3].xwyw;"
"MOV result.color.w, c[1].y;"
"END";

int CLGLRenderer::gl_status = CLGLRenderer::CL_GL_NOT_TESTED;

static QList<GLuint *> mGarbage;

CLGLRenderer::CLGLRenderer(CLVideoWindowItem *vw) :
    clampConstant(GL_CLAMP),
    isNonPower2(false),
    isSoftYuv2Rgb(false),
    m_forceSoftYUV(false),
    m_textureUploaded(false),
    m_stride_old(0),
    m_height_old(0),
    m_videoTextureReady(false),
    m_brightness(0),
    m_contrast(0),
    m_hue(0),
    m_saturation(0),
    m_painterOpacity(1.0),
    m_gotnewimage(false),
    m_needwait(true),
    m_videowindow(vw),
    m_inited(false),
    m_curImg(0),
    m_videoWidth(0),
    m_videoHeight(0)

{
    applyMixerSettings(m_brightness, m_contrast, m_hue, m_saturation);

    (void)CLGLRenderer::getMaxTextureSize();
}

int CLGLRenderer::checkOpenGLError() const
{
    int err = glGetError();
    if (GL_NO_ERROR != err)
    {
        const char* errorString = reinterpret_cast<const char *>(gluErrorString(err));
        if (errorString)
            cl_log.log(QLatin1String("OpenGL Error: "), QString::fromLatin1(errorString), cl_logERROR);
    }
    return err;
}

CLGLRenderer::~CLGLRenderer()
{
    if (m_textureUploaded)
    {
        //glDeleteTextures(3, m_texture);

        // I do not know why but if I glDeleteTextures here some items on the other view might become green( especially if we animate them a lot )
        // not sure I i do something wrong with opengl or it's bug of QT. for now can not spend much time on it. but it needs to be fixed.

        GLuint *heap = new GLuint[3];
        memcpy(heap, m_texture, sizeof(m_texture));
        mGarbage.append(heap); // to delete later
    }
}

void CLGLRenderer::clearGarbage()
{
    foreach (GLuint *heap, mGarbage)
    {
        glDeleteTextures(3, heap);
        delete [] heap;
    }

    mGarbage.clear();
}

void CLGLRenderer::init(bool msgbox)
{
    if (m_inited)
        return;

//    makeCurrent();

    glProgramStringARB = (_glProgramStringARB) QGLContext::currentContext()->getProcAddress(QLatin1String("glProgramStringARB"));
    glBindProgramARB = (_glBindProgramARB) QGLContext::currentContext()->getProcAddress(QLatin1String("glBindProgramARB"));
    glDeleteProgramsARB = (_glDeleteProgramsARB) QGLContext::currentContext()->getProcAddress(QLatin1String("glDeleteProgramsARB"));
    glGenProgramsARB = (_glGenProgramsARB) QGLContext::currentContext()->getProcAddress(QLatin1String("glGenProgramsARB"));
    glProgramLocalParameter4fARB = (_glProgramLocalParameter4fARB) QGLContext::currentContext()->getProcAddress(QLatin1String("glProgramLocalParameter4fARB"));
    glActiveTexture = (_glActiveTexture) QGLContext::currentContext()->getProcAddress(QLatin1String("glActiveTexture"));

    if (!glActiveTexture)
        CL_LOG(cl_logWARNING) cl_log.log(QLatin1String("GL_ARB_multitexture not supported"), cl_logWARNING);


    // OpenGL info
    const char *extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    if (extensions)
        CL_LOG(cl_logWARNING) cl_log.log(QLatin1String("OpenGL extensions: "), QString::fromLatin1(extensions), cl_logDEBUG1);

    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    if (version)
        CL_LOG(cl_logWARNING) cl_log.log(QLatin1String("OpenGL version: "), QString::fromLatin1(version), cl_logALWAYS);

    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    if (renderer)
        CL_LOG(cl_logWARNING) cl_log.log(QLatin1String("Renderer: "), QString::fromLatin1(renderer), cl_logALWAYS);

    const char *vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
    if (vendor)
        CL_LOG(cl_logWARNING) cl_log.log(QLatin1String("Vendor: "), QString::fromLatin1(vendor), cl_logALWAYS);

    QByteArray ext(extensions);
    QByteArray ver(version);

    //ver = "1.0.7 Microsoft";
    clampConstant = GL_CLAMP;
    if (ext.contains("GL_EXT_texture_edge_clamp"))
    {
        clampConstant = GL_CLAMP_TO_EDGE_EXT;
    }
    else if (ext.contains("GL_SGIS_texture_edge_clamp"))
    {
        clampConstant = GL_CLAMP_TO_EDGE_SGIS;
    }
    else if (ver >= QByteArray("1.2.0"))
    {
        clampConstant = GL_CLAMP_TO_EDGE;
    }

    if (ver <= QByteArray("1.1.0"))
    {
        // Microsoft Generic software
        const QString message = QObject::tr("OpenGL driver is not installed or outdated. Please update video driver for better perfomance.");
        CL_LOG(cl_logWARNING) cl_log.log(message, cl_logWARNING);
        if (msgbox)
            QMessageBox::warning(0, QObject::tr("Important Performance Tip"), message, QMessageBox::Ok, QMessageBox::NoButton);
    }

    bool error = false;
    if (extensions)
    {
        isNonPower2 = ext.contains("GL_ARB_texture_non_power_of_two");

        if (!ext.contains("GL_ARB_fragment_program"))
        {
            CL_LOG(cl_logERROR) cl_log.log(QLatin1String("GL_ARB_fragment_program"), QLatin1String(" not support"), cl_logERROR);
            error = true;
        }
    }

    if (!error)
        error = !(glProgramStringARB && glBindProgramARB && glDeleteProgramsARB && glGenProgramsARB && glProgramLocalParameter4fARB);

    if (!error)
    {
        QMutexLocker locker(&m_programMutex);

        if (!m_programInited)
        {
            glGenProgramsARB(ProgramCount, m_program);

            static const char *code[] = { yv12ToRgb, yuy2ToRgb };

            for (int i = 0; i < ProgramCount; ++i)
            {
                glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_program[i]);

                const GLbyte *gl_src = reinterpret_cast<const GLbyte *>(code[i]);
                glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, qstrlen(code[i]), gl_src);

                if (checkOpenGLError() != GL_NO_ERROR)
                {
                    error = true;
                    break;
                }
            }

            if (error)
            {
                glDeleteProgramsARB(ProgramCount, m_program);
                CL_LOG(cl_logERROR) cl_log.log(QLatin1String("Error compile shader!!!"), cl_logERROR);
            }

            m_programInited = !error;
        }
    }

    //isSoftYuv2Rgb = true;

    if (error)
    {
        CL_LOG(cl_logWARNING) cl_log.log(QLatin1String("OpenGL shader support init failed, use soft yuv->rgb converter!!!"), cl_logWARNING);
        isSoftYuv2Rgb = true;

        // in this first revision we do not support software color transform
        gl_status = CL_GL_NOT_SUPPORTED;

        if (msgbox)
        {
            const QString message = QObject::tr("We have detected that your video card drivers may be not installed or out of date.\n"
                                                "Installing and/or updating your video drivers can substantially increase your system performance when viewing and working with video.\n"
                                                "For easy instructions on how to install or update your video driver, follow instruction at http://tribaltrouble.com/driversupport.php");
            QMessageBox::warning(0, QObject::tr("Important Performance Tip"), message, QMessageBox::Ok, QMessageBox::NoButton);
        }
    }

    if (m_forceSoftYUV)
    {
        isSoftYuv2Rgb = true;
    }
#if 0
    // force CPU yuv->rgb for large textures (due to ATI bug). Only for still images.
    else if (m_videowindow && m_videowindow->getVideoCam()->getDevice()->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT) &&
                (m_width >= MAX_SHADER_SIZE || m_height >= MAX_SHADER_SIZE))
    {
        isSoftYuv2Rgb = true;
    }
#endif

    //if (mGarbage.size() < 80)
    {
        glGenTextures(3, m_texture);
    }
    /*
    else
    {
        GLuint *heap = mGarbage.takeFirst();
        memcpy(m_texture, heap, sizeof(m_texture));
        delete [] heap;
    }
    */

    OGL_CHECK_ERROR("glGenTextures");

    glEnable(GL_TEXTURE_2D);
    OGL_CHECK_ERROR("glEnable");

    gl_status = CL_GL_SUPPORTED;

}

Q_GLOBAL_STATIC(QMutex, maxTextureSizeMutex)

int CLGLRenderer::getMaxTextureSize()
{
    static GLint maxTextureSize = 0;
    if (!maxTextureSize)
    {
        QMutexLocker locker(maxTextureSizeMutex());

        if (!maxTextureSize)
        {
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
            cl_log.log(QLatin1String("Max Texture size: "), maxTextureSize, cl_logALWAYS);
        }
    }

    return maxTextureSize;
}

void CLGLRenderer::beforeDestroy()
{
    QMutexLocker lock(&m_displaySync);
    m_needwait = false;
    if (m_curImg)
        m_curImg->setDisplaying(false);
    m_waitCon.wakeAll();
}

void CLGLRenderer::draw(CLVideoDecoderOutput* img, unsigned int channel)
{
    Q_UNUSED(channel);

    QMutexLocker locker(&m_displaySync);
    //m_abort_drawing = false;

    //m_imageList.enqueue(img);

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
    if (!m_videowindow->isVisible())
        return;

    m_videowindow->needUpdate(true); // sending paint event
}

void CLGLRenderer::waitForFrameDisplayed(int channel)
{
    Q_UNUSED(channel)

    if (m_needwait && m_curImg)
    {
        QMutexLocker lock(&m_displaySync);
        while (m_curImg->isDisplaying())
            m_waitCon.wait(&m_displaySync);
    }
}

void CLGLRenderer::setForceSoftYUV(bool value)
{
    m_forceSoftYUV = value;
}

qreal CLGLRenderer::opacity() const
{
    return m_painterOpacity;
}

void CLGLRenderer::setOpacity(qreal opacity)
{
    m_painterOpacity = opacity;
}

void CLGLRenderer::applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation)
{
    // normalize the values
    m_brightness = brightness * 128;
    m_contrast = contrast + 1.0;
    m_hue = hue * 180.;
    m_saturation = saturation + 1.0;
}

int CLGLRenderer::glRGBFormat() const
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

bool CLGLRenderer::isPixelFormatSupported(PixelFormat pixfmt)
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

bool CLGLRenderer::isYuvFormat() const
{
    return m_format == PIX_FMT_YUV422P || m_format == PIX_FMT_YUV420P || m_format == PIX_FMT_YUV444P;
}

void CLGLRenderer::updateTexture()
{
    //image.saveToFile("test.yuv");

    int w[3] = { m_curImg->linesize[0], m_curImg->linesize[1], m_curImg->linesize[2] };
    int r_w[3] = { m_curImg->width, m_curImg->width / 2, m_curImg->width / 2 }; // real_width / visable
    int h[3] = { m_curImg->height, m_curImg->height / 2, m_curImg->height / 2 };

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
    if (!isSoftYuv2Rgb && isYuvFormat())
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
                int wPow = isNonPower2 ? roundUp(w[i],ROUND_COEFF) : getMinPow2(w[i]);
                int hPow = isNonPower2 ? h[i] : getMinPow2(h[i]);
                // support GL_ARB_texture_non_power_of_two ?

                m_videoCoeffL[i] = 0;
                int round_r_w = roundUp(r_w[i],ROUND_COEFF);
                m_videoCoeffW[i] =  round_r_w / (float) wPow;

                m_videoCoeffH[i] = h[i] / (float) hPow;

                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, wPow, hPow, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampConstant);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampConstant);

                // by default uninitialized YUV texture has green color. Fill right and bottom black bars
                // due to GL_LINEAR filtering, openGL uses some "unvisible" pixels, so it is unvisible pixels MUST be black
                int fillSize = qMax(round_r_w, h[i]) * ROUND_COEFF;
                uchar *staticFiller;
                if (i == 0)
                {
                    if (m_staticYFiller.size() < fillSize)
                    {
                        m_staticYFiller.resize(fillSize);
                        m_staticYFiller.fill(0x10);
                    }
                    staticFiller = &m_staticYFiller[0];
                } else {
                    if (m_staticUVFiller.size() < fillSize)
                    {
                        m_staticUVFiller.resize(fillSize);
                        m_staticUVFiller.fill(0x80);
                    }
                    staticFiller = &m_staticUVFiller[0];
                }

                glPixelStorei(GL_UNPACK_ROW_LENGTH, ROUND_COEFF);
                if (round_r_w < wPow)
                {
                    glTexSubImage2D(GL_TEXTURE_2D, 0,
                        round_r_w,
                        0,
                        qMin(ROUND_COEFF, wPow - round_r_w),
                        h[i],
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, staticFiller);
                    OGL_CHECK_ERROR("glTexSubImage2D");
                }
                if (h[i] < hPow)
                {
                    glTexSubImage2D(GL_TEXTURE_2D, 0,
                        0, h[i],
                        qMin(round_r_w + ROUND_COEFF, wPow),
                        qMin(h[i] + ROUND_COEFF, hPow),
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, staticFiller);
                    OGL_CHECK_ERROR("glTexSubImage2D");
                }
            }

            glPixelStorei(GL_UNPACK_ROW_LENGTH, w[i]);
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            roundUp(r_w[i],ROUND_COEFF),
                            h[i],
                            GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
            OGL_CHECK_ERROR("glTexSubImage2D");
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            OGL_CHECK_ERROR("glPixelStorei");
        }

        m_textureUploaded = true;
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, m_texture[0]);

        //const int wPow = isNonPower2 ? roundUp(w[0]) : getMinPow2(w[0]);
        //const int hPow = isNonPower2 ? h[0] : getMinPow2(h[0]);

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
            const int wPow = isNonPower2 ? roundUp(w[0]/bytesPerPixel,ROUND_COEFF) : getMinPow2(w[0]/bytesPerPixel);
            const int hPow = isNonPower2 ? h[0] : getMinPow2(h[0]);
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
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampConstant);
            OGL_CHECK_ERROR("glTexParameteri");
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampConstant);
            OGL_CHECK_ERROR("glTexParameteri");
        }

        uchar *pixels = m_curImg->data[0];
        if (isYuvFormat())
        {
            int size = 4 * m_curImg->linesize[0] * h[0];
            if (yuv2rgbBuffer.size() < size)
                yuv2rgbBuffer.resize(size);
            pixels = yuv2rgbBuffer.data();
        }

        int lineInPixelsSize = m_curImg->linesize[0];
        switch (m_curImg->format)
        {
        case PIX_FMT_YUV422P:
            yuv422_argb32_mmx(pixels, m_curImg->data[0], m_curImg->data[2], m_curImg->data[1],
                              roundUp(r_w[0],ROUND_COEFF),
                              h[0],
                              4 * m_curImg->linesize[0],
                              m_curImg->linesize[0], m_curImg->linesize[1]);
            break;

        case PIX_FMT_YUV420P:
            yuv420_argb32_mmx(pixels, m_curImg->data[0], m_curImg->data[2], m_curImg->data[1],
                              roundUp(r_w[0],ROUND_COEFF),
                              h[0],
                              4 * m_curImg->linesize[0],
                              m_curImg->linesize[0], m_curImg->linesize[1]);
            break;

        case PIX_FMT_YUV444P:
            yuv444_argb32_mmx(pixels, m_curImg->data[0], m_curImg->data[2], m_curImg->data[1],
                              roundUp(r_w[0],ROUND_COEFF),
                              h[0],
                              4 * m_curImg->linesize[0],
                              m_curImg->linesize[0], m_curImg->linesize[1]);
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

        if (m_videowindow->getVideoCam()->getDevice()->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT))
        {
            // free memory immediately for still images
            yuv2rgbBuffer.clear();
        }
    }

    //glDisable(GL_TEXTURE_2D);
    m_videoTextureReady = true;
    m_gotnewimage = false;

    /*
    if (!isSoftYuv2Rgb)
    {
        glActiveTexture(GL_TEXTURE0);glDisable (GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE1);glDisable (GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE2);glDisable (GL_TEXTURE_2D);
    }
    */
}

void CLGLRenderer::drawVideoTexture(GLuint tex0, GLuint tex1, GLuint tex2, const float* v_array)
{
    float tx_array[8] = {m_videoCoeffL[0], 0.0f,
                        m_videoCoeffW[0], 0.0f,
                        m_videoCoeffW[0], m_videoCoeffH[0],
                        m_videoCoeffL[0], m_videoCoeffH[0]};
                        /**/

    /*
    float tx_array[8] = {0.0f, 0.0f,
        m_videoCoeffW[0], 0.0f,
        m_videoCoeffW[0], m_videoCoeffH[0],
        0.0f, m_videoCoeffH[0]};
        */
    glEnable(GL_TEXTURE_2D);
    OGL_CHECK_ERROR("glEnable");

    if (!isSoftYuv2Rgb && isYuvFormat())
    {
        const Program prog = YV12toRGB;

        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_program[prog]);
        OGL_CHECK_ERROR("glBindProgramARB");
        //loading the parameters
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, m_brightness / 256.0f, m_contrast, qCos(m_hue), qSin(m_hue));
        OGL_CHECK_ERROR("glProgramLocalParameter4fARB");
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, m_saturation, m_painterOpacity, 0.0f, 0.0f);
        OGL_CHECK_ERROR("glProgramLocalParameter4fARB");

        glEnable(GL_FRAGMENT_PROGRAM_ARB);
        OGL_CHECK_ERROR("glEnable");

        glActiveTexture(GL_TEXTURE0);
        OGL_CHECK_ERROR("glActiveTexture");

        glBindTexture(GL_TEXTURE_2D, tex0);
        OGL_CHECK_ERROR("glBindTexture");

        if (prog == YV12toRGB)
        {
            glActiveTexture(GL_TEXTURE1);
            OGL_CHECK_ERROR("glActiveTexture");
            /**/
            glBindTexture(GL_TEXTURE_2D, tex1);
            OGL_CHECK_ERROR("glBindTexture");

            glActiveTexture(GL_TEXTURE2);
            OGL_CHECK_ERROR("glActiveTexture");
            /**/
            glBindTexture(GL_TEXTURE_2D, tex2);
            OGL_CHECK_ERROR("glBindTexture");
        }
    }
    else
    {
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
    /**/

    if (!isSoftYuv2Rgb)
    {
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
        OGL_CHECK_ERROR("glDisable");
        glActiveTexture(GL_TEXTURE0);
        OGL_CHECK_ERROR("glActiveTexture");
    }
}

#if 0
static inline QRect getTextureRect(float textureWidth, float textureHeight,
                                   float windowWidth, float windowHeight, const float sar)
{
    Q_UNUSED(textureWidth);
    Q_UNUSED(textureHeight);
    Q_UNUSED(sar);

    QRect r(0, 0, windowWidth, windowHeight);
    return r;
}
#endif

bool CLGLRenderer::paintEvent(const QRect &r)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //if (m_abort_drawing)
    //    return true;

    if (!m_inited)
    {
        init(gl_status == CL_GL_NOT_TESTED);
        m_inited = true;
    }

    {
        QMutexLocker locker(&m_displaySync);
        if (m_gotnewimage && m_curImg && m_curImg->linesize[0]) {
            m_videoWidth = m_curImg->width;
            m_videoHeight = m_curImg->height;
            updateTexture();
        }
    }
    bool draw = m_videoWidth <= getMaxTextureSize() && m_videoHeight <= getMaxTextureSize();
    if (draw && m_videoWidth > 0 && m_videoHeight > 0)
    {
        QRect temp(r);
        const float v_array[] = { temp.left(), temp.top(), temp.right() + 1, temp.top(), temp.right() + 1, temp.bottom() + 1, temp.left(), temp.bottom() + 1 };
        drawVideoTexture(m_texture[0], m_texture[1], m_texture[2], v_array);
    }

    QMutexLocker locker(&m_displaySync);
    if (m_curImg)
        m_curImg->setDisplaying(false);
    m_waitCon.wakeAll();
    return draw;
}

QSize CLGLRenderer::sizeOnScreen(unsigned int channel) const
{
    return m_videowindow->sizeOnScreen(channel);
}

bool CLGLRenderer::constantDownscaleFactor() const
{
    return m_videowindow->constantDownscaleFactor();
}
