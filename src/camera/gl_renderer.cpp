#include "gl_renderer.h"
#include "../base/log.h"
#include "../base/sleep.h"
#include <cmath> //for sin and cos
#include "ui/videoitem/video_wnd_item.h"
#include "yuvconvert.h"
#include "camera.h"
#include "device/device.h"

#include <QTime>

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

QVector<quint8> CLGLRenderer::m_staticYFiller;
QVector<quint8> CLGLRenderer::m_staticUVFiller;

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
GLint CLGLRenderer::ms_maxTextureSize = 0;
QMutex CLGLRenderer::ms_maxTextureSizeMutex;

QList<GLuint*> CLGLRenderer::mGarbage;

CLGLRenderer::CLGLRenderer(CLVideoWindowItem *vw) :
    clampConstant(GL_CLAMP),
    isNonPower2(false),
    isSoftYuv2Rgb(false),
    m_forceSoftYUV(false),
    m_textureUploaded(false),
    m_stride(0), // in memorry
    m_width(0), // visible width
    m_height(0),
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
    m_inited(false)
{
    applyMixerSettings(m_brightness, m_contrast, m_hue, m_saturation);
}

int CLGLRenderer::checkOpenGLError() const
{
    int err = glGetError();
    if (GL_NO_ERROR != err)
    {
        const char* errorString = reinterpret_cast<const char *>(gluErrorString(err));
        if (errorString)
        {
            cl_log.log("OpenGL Error: ",  errorString, cl_logERROR);
        }
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

        GLuint* heap = new GLuint[3];
        memcpy(heap,m_texture,sizeof(m_texture));
        mGarbage.push_back(heap); // to delete later
    }
}

void CLGLRenderer::clearGarbage()
{
    foreach(GLuint* heap, mGarbage)
    {
        glDeleteTextures(3, heap);
        delete[] heap;
    }

    mGarbage.clear();
}

void CLGLRenderer::getTextureRect(QRect& drawRect, 
                    float textureWidth, float textureHeight,
                    float windowWidth, float windowHeight, const float sar) const
{
    Q_UNUSED(textureWidth);
    Q_UNUSED(textureHeight);
    Q_UNUSED(sar);

    drawRect.setTop(0);
    drawRect.setLeft(0);
    drawRect.setWidth(static_cast<int>(windowWidth));
    drawRect.setHeight(static_cast<int>(windowHeight));
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
    {
        CL_LOG(cl_logWARNING) cl_log.log("GL_ARB_multitexture not supported",cl_logWARNING);
    }

    // OpenGL info
    const char* extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    if (extensions)
    {
        CL_LOG(cl_logWARNING) cl_log.log("OpenGL extensions: ", extensions, cl_logDEBUG1);
    }
    const char* version = reinterpret_cast<const char *>(glGetString(GL_VERSION));

    if (version)
    {
        //CL_LOG(cl_logWARNING) cl_log.log(,cl_logWARNING);
        CL_LOG(cl_logWARNING) cl_log.log("OpenGL version: ", version, cl_logALWAYS);
    }

    const uchar* renderer = glGetString(GL_RENDERER);
    if (renderer)
    {
        CL_LOG(cl_logWARNING) cl_log.log("Renderer: ", reinterpret_cast<const char *>(renderer),cl_logALWAYS);
    }

    const uchar* vendor = glGetString(GL_VENDOR);
    if (vendor)
    {
        CL_LOG(cl_logWARNING) cl_log.log("Vendor: ", reinterpret_cast<const char *>(vendor),cl_logALWAYS);
    }

    //version = "1.0.7";
    clampConstant = GL_CLAMP;
    if (version && QString(version) >= QString("1.2.0"))
    {
        clampConstant = GL_CLAMP_TO_EDGE;
    }
    if (extensions && strstr(extensions, "GL_EXT_texture_edge_clamp"))
    {
        clampConstant = GL_CLAMP_TO_EDGE_EXT;
    }
    else if (extensions && strstr(extensions, "GL_SGIS_texture_edge_clamp"))
    {
        clampConstant = GL_CLAMP_TO_EDGE_SGIS;
    }

    if (version && QString(version) <= "1.1.0")
    { // Microsoft Generic software
        const QString& message = QObject::trUtf8("SLOW_OPENGL");
        CL_LOG(cl_logWARNING) cl_log.log(message ,cl_logWARNING);
        if (msgbox)
        {
            QMessageBox* box = new QMessageBox(QMessageBox::Warning, "Info", message, QMessageBox::Ok, 0);
            box->show();
        }
    }

    bool error = false;
    if (extensions)
    {
        isNonPower2 = strstr(extensions, "GL_ARB_texture_non_power_of_two") != NULL;
        
        const char* fragmentProgram = "GL_ARB_fragment_program";
        if (!strstr(extensions, fragmentProgram))
        {
            CL_LOG(cl_logERROR) cl_log.log(fragmentProgram, " not support" ,cl_logERROR);
            error = true;
        }
    }

    if (!error && glProgramStringARB && glBindProgramARB && glDeleteProgramsARB &&
            glGenProgramsARB && glProgramLocalParameter4fARB)
    {

        QMutexLocker mtx (&m_programMutex);
        if (!m_programInited)
        {
            glGenProgramsARB(2, m_program);

            //==================
            const char *code[] = {yv12ToRgb, yuy2ToRgb};

            for(int i = 0; i < ProgramCount && !error;  ++i)
            {

                glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_program[i]);

                const GLbyte *gl_src = reinterpret_cast<const GLbyte *>(code[i]);
                glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                    strlen(code[i]), gl_src);

                if (checkOpenGLError() != GL_NO_ERROR)
                {
                    error = true;
                }
            }

            if (error)
            {
                glDeleteProgramsARB(2, m_program);
                CL_LOG(cl_logERROR) cl_log.log("Error compile shader!!!", cl_logERROR);
            }
            else
                m_programInited = true;

        }

        //==================

    }
    else
    {
        error = true;
    }

    //isSoftYuv2Rgb = true;

    if (error)
    {
        CL_LOG(cl_logWARNING) cl_log.log("OpenGL shader support init failed, use soft yuv->rgb converter!!!", cl_logWARNING);
        isSoftYuv2Rgb = true;

        // in this first revision we do not support software color transform
        gl_status = CL_GL_NOT_SUPPORTED;

        const QString& message = QObject::trUtf8("This software version supports only GPU(not CPU) color transformation. This video card do not supports shaders(GPU transforms). Please contact to developers to get new software version with YUV=>RGB software transform for your video card. Or update your video card:-)");
        CL_LOG(cl_logWARNING) cl_log.log(message ,cl_logWARNING);
        if (msgbox)
        {
            QMessageBox* box = new QMessageBox(QMessageBox::Warning, "Info", message, QMessageBox::Ok, 0);
            box->show();
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

    //if (mGarbage.count()<80)
    {
        glGenTextures(3, m_texture);
    }
    /*
    else
    {
        GLuint* heap = mGarbage.takeFirst();
        memcpy(m_texture, heap, sizeof(m_texture));
        delete heap;
    }
    */

    OGL_CHECK_ERROR("glGenTextures");

    glEnable(GL_TEXTURE_2D);
    OGL_CHECK_ERROR("glEnable");

    gl_status = CL_GL_SUPPORTED;

}

int CLGLRenderer::getMaxTextureSize()
{
    if (ms_maxTextureSize)
        return ms_maxTextureSize;

    {
        QMutexLocker maxTextureSizeLocker_(&ms_maxTextureSizeMutex);

        if (!ms_maxTextureSize)
        {
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &ms_maxTextureSize); 

            cl_log.log("Max Texture size: ", ms_maxTextureSize, cl_logALWAYS);
        }
    }

    return ms_maxTextureSize;
}

void CLGLRenderer::beforeDestroy()
{
    m_needwait = false;
    m_waitCon.wakeOne();
}

void CLGLRenderer::copyVideoDataBeforePainting(bool copy)
{
    m_copyData = copy;
    if (copy)
    {
        m_abort_drawing = true; // after we cancel wait process we need to abort_draw.
        m_needwait = false;
        m_waitCon.wakeOne();
    }
    else
    {
        m_needwait = true;
    }
}

void CLGLRenderer::draw(CLVideoDecoderOutput& img, unsigned int channel)
{
    Q_UNUSED(channel);

    QMutexLocker locker(&m_mutex);

    m_abort_drawing = false;

    if (m_copyData)
        CLVideoDecoderOutput::copy(&img, &m_image);

    CLVideoDecoderOutput& image =  m_copyData ?  m_image : img;

    m_stride = image.stride1;
    m_height = image.height;
    m_width = image.width;

    m_color = image.out_type;

    if (m_stride != m_stride_old || m_height!=m_height_old || m_color!=m_color_old)
    {
        m_videoTextureReady = false;
        m_stride_old = m_stride;
        m_height_old = m_height;
        m_color_old = m_color;

    }

    m_arrayPixels[0] = image.C1;
    m_arrayPixels[1] = image.C2;
    m_arrayPixels[2] = image.C3;

    m_gotnewimage = true;
    //CLSleep::msleep(15);

    //QTime time;
    //time.restart();

    if (!m_videowindow->isVisible())
        return;

    m_videowindow->needUpdate(true); // sending paint event
    //m_videowindow->update();

    if (m_needwait)
    {
        m_do_not_need_to_wait_any_more = false;

        while (!m_waitCon.wait(&m_mutex, 50)) // unlock the mutex
        {
            if (!m_videowindow->isVisible() || !m_needwait)
                break;

            if (m_do_not_need_to_wait_any_more)
                break; // some times It does not wake up after wakeone is called ; is it a bug?
        }
    }

    //cl_log.log("time =", time.elapsed() , cl_logDEBUG1);

    // after paint had hapened

    //paintEvent
}

void CLGLRenderer::setForceSoftYUV(bool value) 
{ 
    m_forceSoftYUV = value;
}

void CLGLRenderer::setOpacity(qreal opacity)
{
    m_painterOpacity = opacity;
}

int roundUp(int value)
{
    return value % ROUND_COEFF ? (value + ROUND_COEFF - (value % ROUND_COEFF)) : value;
}

int CLGLRenderer::glRGBFormat() const
{
    if (isYuvFormat())
        return GL_RGBA;
    switch(m_color)
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
            return GL_RGBA;
    }
}

bool CLGLRenderer::isPixelFormatSupported(PixelFormat pixfmt) 
{
    static const PixelFormat supportedFormats[] = 
    {
            PIX_FMT_YUV422P, 
            PIX_FMT_YUV420P, 
            PIX_FMT_YUV444P, 
            PIX_FMT_RGBA, 
            PIX_FMT_BGRA, 
            PIX_FMT_RGB24,
            PIX_FMT_BGR24
    };
    for (unsigned i = 0; i < sizeof (supportedFormats)/ sizeof(PixelFormat); ++i)
    {
        if (pixfmt == supportedFormats[i])
            return true;
    }
    return false;
}

bool CLGLRenderer::isYuvFormat() const
{
    return m_color == PIX_FMT_YUV422P || m_color == PIX_FMT_YUV420P || m_color == PIX_FMT_YUV444P;
}

void CLGLRenderer::updateTexture()
{
    //image.saveToFile("test.yuv");

    int w[3] = { m_stride, m_stride / 2, m_stride / 2 };
    int r_w[3] = { m_width, m_width / 2, m_width / 2 }; // real_width / visable
    int h[3] = { m_height, m_height / 2, m_height / 2 };

    if (m_color == PIX_FMT_YUV422P)
        h[1] = h[2] = m_height;

    if (m_color == PIX_FMT_YUV444P)
    {
        h[1] = h[2] = m_height;
        w[1] = w[2] = m_stride;
        r_w[1] = r_w[2] = m_width;
    }
    //int round_width[3] = {roundUp(w[0]), roundUp(w[1]), roundUp(w[2])};
    //int round_width[3] = {roundUp(r_w[0]), roundUp(r_w[1]), roundUp(r_w[2])};

    glEnable(GL_TEXTURE_2D);
    OGL_CHECK_ERROR("glEnable");
    if (!isSoftYuv2Rgb && isYuvFormat())
    {
        // using pixel shader to yuv-> rgb conversion
        for (int i = 0; i < 3; ++i)
        {
            glBindTexture(GL_TEXTURE_2D, m_texture[i]);
            OGL_CHECK_ERROR("glBindTexture");
            const uchar* pixels = m_arrayPixels[i];
            if (!m_videoTextureReady)
            {
                // if support "GL_ARB_texture_non_power_of_two", use default size of texture,
                // else nearest power of two
                int wPow = isNonPower2 ? roundUp(w[i]) : getMinPow2(w[i]);
                int hPow = isNonPower2 ? h[i] : getMinPow2(h[i]);
                // support GL_ARB_texture_non_power_of_two ?

                m_videoCoeffL[i] = 0;
                int round_r_w = roundUp(r_w[i]);
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
                quint8* staticFiller;
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
                            roundUp(r_w[i]),
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

                const int wPow = isNonPower2 ? roundUp(w[0]) : getMinPow2(w[0]);
                const int hPow = isNonPower2 ? h[0] : getMinPow2(h[0]);
                // support GL_ARB_texture_non_power_of_two ?
                m_videoCoeffL[0] = 0;
                m_videoCoeffW[0] =  roundUp(r_w[0]) / (float) wPow;
                m_videoCoeffH[0] = h[0] / (float) hPow;

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

        quint8* pixels = m_arrayPixels[0];
        if (isYuvFormat())
        {
            int size = 4 * m_stride * h[0];
            if (yuv2rgbBuffer.size() != size)
                yuv2rgbBuffer.resize(size);
            pixels = &yuv2rgbBuffer[0];
        }

        int lineInPixelsSize = m_stride;
        if (m_color == PIX_FMT_YUV422P)
        {
            yuv422_argb32_mmx(pixels, m_arrayPixels[0], m_arrayPixels[2], m_arrayPixels[1],
                              roundUp(r_w[0]),
                              h[0],
                              4 * m_stride,
                              m_stride, m_stride / 2);
        }
        else if (m_color == PIX_FMT_YUV420P)
        {
            yuv420_argb32_mmx(pixels, m_arrayPixels[0], m_arrayPixels[2], m_arrayPixels[1],
                              roundUp(r_w[0]),
                              h[0],
                              4 * m_stride,
                              m_stride, m_stride / 2);
        }
        else if (m_color == PIX_FMT_YUV444P)
        {
            yuv444_argb32_mmx(pixels, m_arrayPixels[0], m_arrayPixels[2], m_arrayPixels[1],
                              roundUp(r_w[0]),
                              h[0],
                              4 * m_stride,
                              m_stride, m_stride);
        }
        else if (m_color == PIX_FMT_RGB24 || m_color == PIX_FMT_BGR24) {
            lineInPixelsSize /= 3;
        }
        else {
            lineInPixelsSize /= 4; // RGBA, BGRA
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, lineInPixelsSize);
        OGL_CHECK_ERROR("glPixelStorei");
        glTexSubImage2D(GL_TEXTURE_2D, 0,
            0, 0,
            roundUp(r_w[0]),
            h[0],
            glRGBFormat(), GL_UNSIGNED_BYTE, pixels);

        OGL_CHECK_ERROR("glTexSubImage2D");
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        OGL_CHECK_ERROR("glPixelStorei");

        if (m_videowindow->getVideoCam()->getDevice()->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT))
        {
            // free memory immediatly for still images
            yuv2rgbBuffer.clear();
        }

        /**/
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

        const Program prog =  YV12toRGB ;

        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_program[prog]);
        OGL_CHECK_ERROR("glBindProgramARB");
        //loading the parameters
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, m_brightness / 256.0f, m_contrast, cos(m_hue), sin(m_hue));
        OGL_CHECK_ERROR("glProgramLocalParameter4fARB");
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, m_saturation, m_painterOpacity, 0.0f, 0.0f);
        OGL_CHECK_ERROR("glProgramLocalParameter4fARB");

        glEnable(GL_FRAGMENT_PROGRAM_ARB);
        OGL_CHECK_ERROR("glEnable");

        glActiveTexture(GL_TEXTURE0);
        OGL_CHECK_ERROR("glActiveTexture");

        glBindTexture(GL_TEXTURE_2D, tex0);
        OGL_CHECK_ERROR("glBindTexture");

        if (YV12toRGB == prog)
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
        /**/
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

bool CLGLRenderer::paintEvent(const QRect& r)
{

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (m_abort_drawing)
        return true;

    if (!m_inited)
    {
        init(gl_status == CL_GL_NOT_TESTED);
        m_inited = true;
    }

    QMutexLocker locker(&m_mutex);

    if (m_stride == 0)
        return true;

    bool draw = (m_width <= getMaxTextureSize()) && (m_height <= getMaxTextureSize());

    if (draw)
    {
        if (m_gotnewimage)    updateTexture();

        //m_painterOpacity = 0.3;
        //m_painterOpacity = 1.0;
        QRect temp;
        //QRect r(0,0,m_videowindow->width(),m_videowindow->height());
        //float sar = 1.0f;

        temp.setLeft(r.left());    temp.setTop(r.top());
        temp.setWidth(r.width());
        temp.setHeight(r.height());

        //getTextureRect(temp, m_stride, m_height, r.width(), r.height(), sar);

        const float v_array[] = { temp.left(), temp.top(), temp.right()+1, temp.top(), temp.right()+1, temp.bottom()+1, temp.left(), temp.bottom()+1 };
        drawVideoTexture(m_texture[0], m_texture[1], m_texture[2], v_array);
    }
    else
    {
        draw = draw;
    }

    m_waitCon.wakeOne();
    m_do_not_need_to_wait_any_more = true;

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
