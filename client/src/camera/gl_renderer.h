#ifndef clgl_renderer_12_29
#define clgl_renderer_12_29

#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include <QtOpenGL/qgl.h>

#include "abstractrenderer.h"

class CLVideoWindowItem;

class CLGLRenderer : public CLAbstractRenderer
{
    friend class CLVideoWindowItem;

public:
    enum CLGLDrawHardwareStatus
    {
        CL_GL_NOT_TESTED,
        CL_GL_SUPPORTED,
        CL_GL_NOT_SUPPORTED
    };

    CLGLRenderer(CLVideoWindowItem *vw);
    ~CLGLRenderer();

    static int getMaxTextureSize();

    void draw(CLVideoDecoderOutput &image, unsigned int channel);

    bool paintEvent(const QRect &r);

    virtual void beforeDestroy();

    QSize sizeOnScreen(unsigned int channel) const;
    bool constantDownscaleFactor() const;

    qreal opacity() const;
    void setOpacity(qreal opacity);

    void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation);

    static bool isPixelFormatSupported(PixelFormat pixfmt);

private:
    static int gl_status;

    void init(bool msgbox);
    static void clearGarbage();

private:
#ifndef Q_OS_WIN
    #define APIENTRY
#endif

    // ARB_fragment_program
    typedef void (APIENTRY *_glProgramStringARB) (GLenum, GLenum, GLsizei, const GLvoid *);
    typedef void (APIENTRY *_glBindProgramARB) (GLenum, GLuint);
    typedef void (APIENTRY *_glDeleteProgramsARB) (GLsizei, const GLuint *);
    typedef void (APIENTRY *_glGenProgramsARB) (GLsizei, GLuint *);
    typedef void (APIENTRY *_glProgramLocalParameter4fARB) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
    typedef void (APIENTRY *_glActiveTexture) (GLenum);
    _glProgramStringARB glProgramStringARB;
    _glBindProgramARB glBindProgramARB;
    _glDeleteProgramsARB glDeleteProgramsARB;
    _glGenProgramsARB glGenProgramsARB;
    _glProgramLocalParameter4fARB glProgramLocalParameter4fARB;
    _glActiveTexture glActiveTexture;

    int checkOpenGLError() const;

    int getMinPow2(int value) const
    {
        int result = 1;
        while (value > result)
            result<<=1;

        return result;
    }

    void drawVideoTexture(GLuint tex0, GLuint tex1, GLuint tex2, const float* v_array);
    void updateTexture();
    void setForceSoftYUV(bool value);
    bool isYuvFormat() const;
    int glRGBFormat() const;

private:
    GLint clampConstant;
    bool isNonPower2;
    bool isSoftYuv2Rgb;

    enum Program
    {
        YV12toRGB = 0,
        YUY2toRGB = 1,
        ProgramCount = 2
    };

    static QMutex m_programMutex;
    static bool m_programInited;
    static GLuint m_program[ProgramCount];
    GLuint m_texture[3];
    bool m_forceSoftYUV;

    QVector<uchar> yuv2rgbBuffer;

    bool m_textureUploaded;

    int m_stride, // in memory
        m_width, // visible width
        m_height,

        m_stride_old,
        m_height_old;

    unsigned char *m_arrayPixels[3];

    PixelFormat m_color, m_color_old;

    float m_videoCoeffL[4];
    float m_videoCoeffW[4];
    float m_videoCoeffH[4];

    bool m_videoTextureReady;

    qreal m_brightness;
    qreal m_contrast;
    qreal m_hue;
    qreal m_saturation;
    qreal m_painterOpacity;

    mutable QMutex m_mutex; // to avoid call paintEvent() more than once at the same time
    QWaitCondition m_waitCon;
    bool m_gotnewimage;

    bool m_needwait;

    CLVideoWindowItem *const m_videowindow;

    CLVideoDecoderOutput m_image;
    bool m_abort_drawing;

    bool m_do_not_need_to_wait_any_more;

    bool m_inited;

    static QVector<quint8> m_staticYFiller;
    static QVector<quint8> m_staticUVFiller;
};

#endif //clgl_renderer_12_29
